#include<assert.h>
#include<string.h>
#include"common.h"

// These actually don't do much, we just want to verify that the macros will
// compile.
static void TestAnnotations(void)
{
   int x = 0;

   if( LIKELY(rand() >= 0) )
      x |= 1;
   else
      x |= 2;

   if( UNLIKELY(rand() < 0) )
      x |= 0x20;
   else
      x |= 0x10;

   if( rand() < 0 )
   {
      UNREACHABLE();
      x |= 0x200;
   }
   else
   {
      x |= 0x100;
   }

   assert(x == 0x111);
}

static void TestRand(void)
{
   int bucket[256];

   for(int bucket_count = 2; bucket_count < 256; bucket_count++)
   {
      memset(bucket, 0, sizeof(int) * bucket_count);
      for(int i = 0; i < 0x1000; i++)
      {
         const int r = RAND(bucket_count - 1);
         assert(r >= 0);
         assert(r < bucket_count);
         bucket[r]++;
      }

      // Verify that every bucket has collected some value.
      // We don't check anything about distribution, though.
      for(int i = 0; i < bucket_count; i++)
         assert(bucket[i] > 0);
   }

   // Verify that zero is an acceptable parameter.
   const int r = RAND(0);
   assert(r == 0);

   // Verify that large ranges are acceptable.
   int generated_high_value = 0;
   for(int i = 0; i < 1000; i++)
   {
      if( RAND(0x20000) > 0x1c000 )
      {
         generated_high_value = 1;
         break;
      }
   }
   assert(generated_high_value);

   int generated_low_value = 0;
   for(int i = 0; i < 1000; i++)
   {
      if( RAND_RANGE(-0x20000, 0) < -0x1c000 )
      {
         generated_low_value = 1;
         break;
      }
   }
   assert(generated_low_value);

   generated_high_value = generated_low_value = 0;
   for(int i = 0; i < 1000; i++)
   {
      const int r = RAND_RANGE(-0x20000, 0x20000);
      if( r < -0x1c000 )
      {
         generated_low_value = 1;
         if( generated_high_value )
            break;
      }
      if( r > 0x1c000 )
      {
         generated_high_value = 1;
         if( generated_low_value )
            break;
      }
   }
   assert(generated_high_value && generated_low_value);
}

static void TestRandRange(void)
{
   for(int i = -4; i <= 4; i++)
   {
      const int r = RAND_RANGE(i, i);
      assert(r == i);
   }

   int bucket[16];
   for(int x0 = -32; x0 <= 32; x0++)
   {
      for(int bucket_count = 1; bucket_count < 16; bucket_count++)
      {
         memset(bucket, 0, sizeof(int) * bucket_count);
         for(int i = 0; i < 0x1000; i++)
         {
            const int r = RAND_RANGE(x0, x0 + bucket_count - 1);
            assert(r >= x0);
            assert(r < x0 + bucket_count);
            bucket[r - x0]++;
         }

         for(int i = 0; i < bucket_count; i++)
            assert(bucket[i] > 0);
      }
   }
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;

   srand(1);
   TestAnnotations();
   TestRand();
   TestRandRange();
   return 0;
}
