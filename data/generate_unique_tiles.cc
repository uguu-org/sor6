// Given an input PNG, output a PNG with unique tiles, plus a text file with
// tile indices to reconstruct the original input.
//
// ./generate_unique_tiles {input.png} {output.png} {output.txt}

#include<png.h>
#include<stdio.h>
#include<string.h>

#include<array>
#include<memory>
#include<string_view>
#include<unordered_map>
#include<vector>

#ifdef _WIN32
   #include<fcntl.h>
   #include<io.h>
   #include<unistd.h>
#endif

namespace {

// Tile size.
static constexpr int kTileSize = 16;

// Set to true for full color, false for grayscale.
static constexpr bool kFullColor = false;

// Bytes per pixel.
static constexpr int kBytesPerPixel = kFullColor ? 4 : 2;

// A single tile worth of pixels.
struct TileBlock
{
   std::array<std::string_view, kTileSize> rows;
};

// Hash function for tile pixels.
struct HashTileBlock
{
   size_t operator()(const TileBlock &block) const
   {
      size_t seed = 0;
      for(const std::string_view &row : block.rows)
      {
         // hash_combine from boost 1.33
         // https://www.boost.org/doc/libs/latest/libs/container_hash/doc/html/hash.html#notes_hash_combine
         seed ^= hasher(row) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
      }
      return seed;
   }

   std::hash<std::string_view> hasher;
};

// Equivalence function for tile pixels.
struct EqTileBlock
{
   bool operator()(const TileBlock &a, const TileBlock &b) const
   {
      return a.rows == b.rows;
   }
};

// Unique tiles to 0-based tile index.
using TileBlockSet =
   std::unordered_map<TileBlock, int, HashTileBlock, EqTileBlock>;

// Input pixels reconstructed as tiles.
struct TiledImage
{
   TileBlockSet unique_tiles;
   std::vector<std::vector<int>> tile_indices;
};

// Create a TileBlock from a region of pixels.
static TileBlock CreateBlock(int width, const uint8_t *pixels, int x, int y)
{
   TileBlock block;

   for(int i = 0; i < kTileSize; i++)
   {
      block.rows[i] = std::string_view(
         reinterpret_cast<const char*>(pixels +
                                       ((y + i) * width + x) * kBytesPerPixel),
         kTileSize * kBytesPerPixel);
   }
   return block;
}

// Check if a block is completely blank.
static bool IsTransparent(const TileBlock &block)
{
   for(int y = 0; y < kTileSize; y++)
   {
      for(char c : block.rows[y])
      {
         if( c != 0 )
            return false;
      }
   }
   return true;
}

// Load tiles to memory.
static TiledImage LoadTiles(int width, int height, const uint8_t *pixels)
{
   TiledImage image;

   image.tile_indices.reserve(height / kTileSize);
   for(int y = 0; y < height; y += kTileSize)
   {
      std::vector<int> row;
      row.reserve(width / kTileSize);
      for(int x = 0; x < width; x += kTileSize)
      {
         const TileBlock block = CreateBlock(width, pixels, x, y);
         if( IsTransparent(block) )
         {
            row.push_back(-1);
         }
         else
         {
            auto p = image.unique_tiles.insert(std::make_pair(
               block, image.unique_tiles.size()));
            row.push_back(p.first->second);
         }
      }
      image.tile_indices.push_back(std::move(row));
   }
   return image;
}

// Write unique tiles pixels to output, returns true on success.
static bool WriteTiles(const TileBlockSet &tiles, const char *output)
{
   // Initialize blank output.
   static constexpr int kOutputWidth = 1024;
   static constexpr int kRowSize = kOutputWidth / kTileSize;
   const int row_count = (tiles.size() + kRowSize - 1) / kRowSize;

   png_image image;
   memset(&image, 0, sizeof(png_image));
   image.version = PNG_IMAGE_VERSION;
   image.flags |= PNG_IMAGE_FLAG_FAST;
   image.format = kFullColor ? PNG_FORMAT_RGBA : PNG_FORMAT_GA;
   image.width = kOutputWidth;
   image.height = row_count * kTileSize;

   std::unique_ptr<uint8_t[]> pixels(new uint8_t[PNG_IMAGE_SIZE(image)]);
   memset(pixels.get(), 0, PNG_IMAGE_SIZE(image));

   // Copy pixels.
   for(const auto &[block, index] : tiles)
   {
      const int x = (index % kRowSize) * kTileSize;
      const int y = (index / kRowSize) * kTileSize;
      for(int i = 0; i < kTileSize; i++)
      {
         memcpy(pixels.get() + ((y + i) * kOutputWidth + x) * kBytesPerPixel,
                block.rows[i].data(),
                block.rows[i].size());
      }
   }

   // Write output.
   if( strcmp(output, "-") == 0 )
   {
      return png_image_write_to_stdio(
                &image, stdout, 0, pixels.get(), 0, nullptr) != 0;
   }
   return png_image_write_to_file(
             &image, output, 0, pixels.get(), 0, nullptr) != 0;
}

// Write tile indices to output, returns true on success.
static bool WriteTileIndices(const std::vector<std::vector<int>> &tile_indices,
                             const char *output)
{
   FILE *outfile;

   if( strcmp(output, "-") == 0 )
   {
      outfile = stdout;
   }
   else
   {
      if( (outfile = fopen(output, "wb")) == nullptr )
      {
         perror("fopen error: ");
         return false;
      }
   }

   for(const std::vector<int> &row : tile_indices)
   {
      for(int cell : row)
         fprintf(outfile, "%d,", cell);
      fputc('\n', outfile);
   }

   if( outfile != stdout )
   {
      if( fclose(outfile) != 0 )
      {
         perror("fclose error: ");
         return false;
      }
   }
   return true;
}

}  // namespace

int main(int argc, char **argv)
{
   if( argc != 4 )
   {
      return fprintf(stderr, "%s {input.png} {output.png} {output.txt}\n",
                     *argv);
   }
   if( strcmp(argv[2], "-") == 0 && strcmp(argv[3], "-") == 0 )
   {
      fputs("At most one of {output.png} or {output.txt} can be \"-\", "
            "not both.\n", stderr);
      return 1;
   }
   #ifdef _WIN32
      setmode(STDIN_FILENO, O_BINARY);
      setmode(STDOUT_FILENO, O_BINARY);
   #endif

   // Load input.
   png_image input_image;
   memset(&input_image, 0, sizeof(png_image));
   input_image.version = PNG_IMAGE_VERSION;
   if( strcmp(argv[1], "-") == 0 )
   {
      if( !png_image_begin_read_from_stdio(&input_image, stdin) )
      {
         fprintf(stderr, "%s: Error reading from stdin\n", argv[1]);
         return 1;
      }
   }
   else
   {
      if( !png_image_begin_read_from_file(&input_image, argv[1]) )
      {
         fprintf(stderr, "%s: Read error\n", argv[1]);
         return 1;
      }
   }
   if( input_image.width % kTileSize != 0 ||
       input_image.height % kTileSize != 0 )
   {
      fprintf(stderr, "%s: Input dimensions is not a multiple of %d: (%d,%d)\n",
              argv[1],
              kTileSize,
              input_image.width,
              input_image.height);
      return 1;
   }

   input_image.format = kFullColor ? PNG_FORMAT_RGBA : PNG_FORMAT_GA;
   std::unique_ptr<uint8_t[]> input_pixels(
      new uint8_t[PNG_IMAGE_SIZE(input_image)]);
   if( !png_image_finish_read(&input_image, nullptr, input_pixels.get(),
                              0, nullptr) )
   {
      fprintf(stderr, "%s: Error loading pixels\n", argv[1]);
      return 1;
   }

   const TiledImage tiled_image = LoadTiles(
      input_image.width, input_image.height, input_pixels.get());
   if( tiled_image.unique_tiles.empty() )
   {
      fprintf(stderr, "%s: Input is completely transparent\n", argv[1]);
      return 1;
   }

   if( !WriteTiles(tiled_image.unique_tiles, argv[2]) )
   {
      fprintf(stderr, "%s: Error writing tiles\n", argv[2]);
      return 1;
   }
   if( !WriteTileIndices(tiled_image.tile_indices, argv[3]) )
   {
      fprintf(stderr, "%s: Error writing tile indices\n", argv[3]);
      return 1;
   }
   return 0;
}
