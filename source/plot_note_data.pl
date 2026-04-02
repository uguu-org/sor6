#!/usr/bin/perl -w
# Convert sor*.txt or sor*.m4 files into HTML with notes plotted in inline
# SVG graphics.  This is used to verify note transcription.
#
# perl plot_note_data.pl [key_signature] {input.txt} > {output.html}
#
# Optional key_signature sets the key signature.  Use "2s" for two sharps or
# "1b" for one flat, etc.  Default uses C major key.  There are no provisions
# for changing key signature in the middle of a song, you will need to run
# this tool multiple times to verify songs with multiple key signatures.
#
# This is a rudimentary tool for converting tab data to something that
# resembles a score, so that it's easier to verify that we have transcribed
# all the note pitches correctly.  It doesn't do anything for timing since
# our tab data doesn't contain enough timing data to reconstruct the
# original notes, and it's better to make it obvious that the timing data is
# completely missing rather than having to guess where the tool might have
# gotten it right.
#
# I needed this tool because most of the tab data found on the internet are
# actually wrong, but subtly wrong in a way that the songs *mostly* sound OK
# and you won't really notice unless you have heard what the songs are
# supposed to sound like.  There is one particular site where the tab data
# appear to be mostly produced by someone who doesn't understand that
# accidentals will persist until the end of measure, and then there is
# another site where some random measure just get duplicated for no reason.
# I didn't want my game to have these half-hearted transcriptions, and I
# have taken the time to verify transcriptions against printed material that
# I have on hand where available.  See comments attached to each song for
# more information.


use strict;

# Number of guitar strings.
use constant STRING_COUNT => 6;

# Score dimensions in pixels.
use constant NOTE_WIDTH => 14;
use constant NOTE_HEIGHT => 11;
use constant CELL_WIDTH => 10;
use constant LINE_HEIGHT => 18;


# Output a single line, with line number.
sub output_line($$)
{
   my ($line, $line_number) = @_;

   printf '<span class="line_number">%4d </span>', $line_number;

   $line =~ s/&/&amp;/g;
   $line =~ s/</&lt;/g;
   $line =~ s/>/&gt;/g;
   print $line;
}

# Convert note to number of semitones away from C3.
# - Returns negative number if input is below C3.
# - Returns positive number if input is above C3.
# - Returns zero if input is C3.
sub note_to_pitch($)
{
   my ($note) = @_;

   $note =~ /([A-G])(\d+)/ or die $!;
   my @offset = (0, 2, 4, 5, 7, 9, 11);
   my $pitch = $offset[index("CDEFGAB", $1)] + $2 * 12;
   return $pitch - 36;
}

# Initialize pitch and modifier for each staff line.
sub init_staff($$$)
{
   my ($key_signature, $pitches, $modifiers) = @_;

   # Assign pitches for C major.
   my @line_notes =
   (
      "D2", "E2",    # -4 ledger line.
      "F2", "G2",    # -3 ledger line.
      "A2", "B2",    # -2 ledger line.
      "C3", "D3",    # -1 ledger line.
      "E3", "F3",    # Line 1.
      "G3", "A3",    # Line 2.
      "B3", "C4",    # Line 3.
      "D4", "E4",    # Line 4.
      "F4", "G4",    # Line 5.
      "A4", "B4",    # +1 ledger line.
      "C5", "D5",    # +2 ledger line.
      "E5", "F5",    # +3 ledger line.
      "G5", "A5",    # +4 ledger line.
      "B5", "C6",    # +5 ledger line.
   );

   # Generate modifiers for each line.
   @$modifiers = ();
   if( $key_signature =~ /^(\d)[#s]/ )
   {
      my $affected_notes = substr("FCGDAEB", 0, $1);
      foreach my $note (@line_notes)
      {
         push @$modifiers,
              (index($affected_notes, substr($note, 0, 1)) >= 0 ? 1 : 0);
      }
   }
   elsif( $key_signature =~ /^(\d)b/ )
   {
      my $affected_notes = substr("BEADGCF", 0, $1);
      foreach my $note (@line_notes)
      {
         push @$modifiers,
              (index($affected_notes, substr($note, 0, 1)) >= 0 ? -1 : 0);
      }
   }
   else
   {
      for(my $i = 0; $i < scalar @line_notes; $i++)
      {
         push @$modifiers, 0;
      }
   }

   # Convert notes to pitches.
   foreach my $note (@line_notes)
   {
      push @$pitches, note_to_pitch($note);
   }
}

# Draw key signature.
sub draw_key_signature($$)
{
   my ($key_signature, $y_offset) = @_;

   if( $key_signature =~ /^(\d)[#s]/ )
   {
      my @position = (0, 1.5, -0.5, 1, 2.5, 0.5, 2);
      for(my $i = 0; $i < $1; $i++)
      {
         my $x = $i * CELL_WIDTH;
         my $y = $position[$i] * LINE_HEIGHT + $y_offset;
         print <<"EOT";
<text class="modifier" x="$x" y="$y">&#x266F;</text>
EOT
      }
   }
   elsif( $key_signature =~ /^(\d)b/ )
   {
      my @position = (2, 0.5, 2.5, 1, 3, 1.5, 3.5);
      for(my $i = 0; $i < $1; $i++)
      {
         my $x = $i * CELL_WIDTH;
         my $y = $position[$i] * LINE_HEIGHT + $y_offset;
         print <<"EOT";
<text class="modifier" x="$x" y="$y">&#x266D;</text>
EOT
      }
   }
}

# Convert notes to list of (column, line, accidental) tuples, sorted by column.
#
# Line is indexed from top to bottom, such that the topmost line on the staff
# (the one with highest pitch) is 0.  Ledger lines above that will get negative
# numbers.  Bottommost line (before ledger lines are added) is 4.  Positions
# between two lines will have a fractional part of 0.5.
sub convert_tab_to_plot($$$$$)
{
   my ($pitches, $modifiers, $tuning, $ruler, $tab) = @_;

   # Make a local copy of the pitch modifiers.  These are updated
   # when accidentals are encountered, and reset across bar lines.
   my @local_modifiers = @$modifiers;

   my @plot_notes = ();
   for(my $i = 0; $i < length($ruler); $i++)
   {
      if( substr($$tab[0], $i, 1) eq "|" )
      {
         @local_modifiers = @$modifiers;
      }
      if( substr($ruler, $i, 1) ne "+" )
      {
         next;
      }
      for(my $j = 0; $j < STRING_COUNT; $j++)
      {
         next unless substr($$tab[$j], $i) =~ /^(\d+)/;
         my $note_pitch = $$tuning[$j] + $1;

         # Three pass heuristic for assigning a line to the note.
         #
         # Here we are taking a greedy approach of assigning the first line
         # that fits, applying accidentals as needed.  The end result is
         # honestly quite ugly.  Professionally engraved scores tend look at
         # the measure as a whole and work harder to minimize the number of
         # accidentals, but we don't do any of that here.

         # First pass: find line with exact pitch, taking existing modifiers
         # into account.
         my $line = undef;
         my $modifier = "";
         for(my $k = 0; $k < scalar @$pitches; $k++)
         {
            if( $$pitches[$k] + $local_modifiers[$k] == $note_pitch )
            {
               $line = $k;
               last;
            }
         }

         # Second pass: find line that can match pitch if current modifier
         # is removed.
         unless( defined($line) )
         {
            for(my $k = 0; $k < scalar @$pitches; $k++)
            {
               if( $$pitches[$k] == $note_pitch )
               {
                  if( $local_modifiers[$k] != 0 )
                  {
                     $line = $k;
                     $modifier = "n";
                     $local_modifiers[$k] = 0;
                     last;
                  }
               }
            }
         }

         # Third pass: find line with matching pitch by applying sharp or flat.
         unless( defined($line) )
         {
            for(my $k = 0; $k < scalar @$pitches; $k++)
            {
               if( $$pitches[$k] + 1 == $note_pitch )
               {
                  if( $local_modifiers[$k] != 1 )
                  {
                     $line = $k;
                     $modifier = "s";
                     $local_modifiers[$k] = 1;
                     last;
                  }
               }
               elsif( $$pitches[$k] - 1 == $note_pitch )
               {
                  if( $local_modifiers[$k] != -1 )
                  {
                     $line = $k;
                     $modifier = "b";
                     $local_modifiers[$k] = -1;
                     last;
                  }
               }
            }
         }

         defined($line) or die;

         # Convert line position.  Lowest pitch is at 4 ledger lines down,
         # or 8 lines down from topmost staff line.  Thus vertical line
         # position is 8 minus line index.
         $line = 8.0 - ($line / 2.0);

         # Add note.
         push @plot_notes, [$i, $line, $modifier];
      }
   }
   return @plot_notes;
}

# Output plots for a set of notes.
sub output_plot($$$$)
{
   my ($key_signature, $tuning, $ruler, $tab) = @_;

   # Initialize staff.
   my (@pitches, @modifiers);
   init_staff($key_signature, \@pitches, \@modifiers);
   (scalar @pitches) == (scalar @modifiers) or die;

   # Reserve area for key signature.
   my $key_signature_width = 0;
   if( $key_signature ne "" )
   {
      $key_signature =~ /^(\d)/ or die;
      $key_signature_width = CELL_WIDTH * ($1 + 2);
   }

   # Find vertical range.
   my @plot_notes =
      convert_tab_to_plot(\@pitches, \@modifiers, $tuning, $ruler, $tab);
   my $top_line = 0;
   my $bottom_line = 4;
   foreach my $p (@plot_notes)
   {
      my ($t, $n, $a) = @$p;
      if( $top_line > $n ) { $top_line = $n; }
      if( $bottom_line < $n ) { $bottom_line = $n; }
   }

   my $width = $key_signature_width + (length($$tab[0]) + 1) * CELL_WIDTH;
   my $height = ($bottom_line - $top_line + 2) * LINE_HEIGHT;
   my $x_offset = $key_signature_width;
   my $y_offset = (-$top_line + 1) * LINE_HEIGHT;

   # End text section and output SVG header.
   print <<"EOT";
</pre><br>
<svg width="$width" height="$height">
EOT

   # Draw staff lines and key signature.
   for(my $i = 0; $i < 5; $i++)
   {
      my $y = $i * LINE_HEIGHT + $y_offset;
      my $x2 = $key_signature_width + length($$tab[0]) * CELL_WIDTH;
      print <<"EOT";
<line class="staff" x1="0" y1="$y" x2="$x2" y2="$y" />
EOT
   }
   draw_key_signature($key_signature, $y_offset);

   # Draw bars.
   for(my $i = 1; $i < length($$tab[0]); $i++)
   {
      if( substr($$tab[0], $i, 1) eq "|" )
      {
         my $x = ($i + 1) * CELL_WIDTH + $x_offset;
         my $y1 = $y_offset;
         my $y2 = $y1 + LINE_HEIGHT * 4;
         print <<"EOT";
<line class="staff" x1="$x" y1="$y1" x2="$x" y2="$y2" />
EOT
      }
   }

   # Plot notes.
   my $x_cursor = -1;
   my $topmost_ledger = 0;
   my $bottommost_ledger = 4;
   foreach my $n (@plot_notes)
   {
      my ($column, $line, $modifier) = @$n;

      # Reset ledger lines when we have moved on to a new column.
      if( $x_cursor != $column )
      {
         $x_cursor = $column;
         $topmost_ledger = 0;
         $bottommost_ledger = 4;
      }

      # Draw ledger lines.
      while( $topmost_ledger > $line + 0.5 )
      {
         $topmost_ledger--;
         my $x1 = ($column + 0.5) * CELL_WIDTH + $x_offset - CELL_WIDTH;
         my $x2 = $x1 + CELL_WIDTH * 2;
         my $y = $topmost_ledger * LINE_HEIGHT + $y_offset;
         print <<"EOT";
<line class="staff" x1="$x1" y1="$y" x2="$x2" y2="$y" />
EOT
      }
      while( $bottommost_ledger < $line - 0.5 )
      {
         $bottommost_ledger++;
         my $x1 = ($column + 0.5) * CELL_WIDTH + $x_offset - CELL_WIDTH;
         my $x2 = $x1 + CELL_WIDTH * 2;
         my $y = $bottommost_ledger * LINE_HEIGHT + $y_offset;
         print <<"EOT";
<line class="staff" x1="$x1" y1="$y" x2="$x2" y2="$y" />
EOT
      }

      # Draw note.
      if( $modifier eq "" )
      {
         my $cx = ($column + 0.5) * CELL_WIDTH + $x_offset;
         my $cy = $line * LINE_HEIGHT + $y_offset;
         my $rx = NOTE_WIDTH / 2;
         my $ry = NOTE_HEIGHT / 2;
         print <<"EOT";
<ellipse class="note" cx="$cx" cy="$cy" rx="$rx" ry="$ry" />
EOT
      }
      else
      {
         my $cx = ($column + 0.5) * CELL_WIDTH + $x_offset;
         my $cy = $line * LINE_HEIGHT + $y_offset;
         my $rx = NOTE_WIDTH / 2;
         my $ry = NOTE_HEIGHT / 2;
         my $tx = $cx - NOTE_WIDTH / 2 - CELL_WIDTH;
         my $t = $modifier eq "s" ? "&#x266F;"
                                  : $modifier eq "b" ? "&#x266D;"
                                                     : "&#x266E;";

         print <<"EOT";
<ellipse class="note" cx="$cx" cy="$cy" rx="$rx" ry="$ry" />
<text class="modifier" x="$tx" y="$cy">$t</text>
EOT
      }
   }

   # Output footer.
   print "</svg>\n<pre>";
}


# Get optional key signature from command line.
my $key_signature = "";
if( $#ARGV >= 0 && $ARGV[0] =~ /^[1-7][#sb]$/ )
{
   $key_signature = shift @ARGV;
}

# Output header, with style sheets for plot elements.
#
# Special features:
# - Modifier text is translated a few pixels down, such that the Y position
#   in the <text> elements is roughly the center of those modifiers.  This
#   allows the modifiers to align with the notes.
#
# - Notes are drawn with hollow ellipses, instead of filled shapes.  Filled
#   shapes would have been visually more accurate because most of the notes
#   are quarter notes, but since we don't have timing information anyway, I
#   just drew all the notes as whole notes.  This also makes them more
#   readable when the notes overlap vertically.
print <<EOT;
<html><head>
<style>
.line_number { color:#f00; user-select:none; }
.modifier { font: 20px sans-serif; transform: translate(0, 6px); }
.staff { fill:none; stroke:#000; stroke-width:1px; }
.note { fill:none; stroke:#000; stroke-width:1px; }
</style>
<body><pre>
EOT

my @tuning = ();
my $line_number = 0;
while( my $line = <> )
{
   $line_number++;
   if( $line =~ /^tuning\s*=\s*([a-gA-G]{6})/ )
   {
      # Set tuning.
      if( uc($1) eq "EADGBE" )
      {
         @tuning =
         (
            note_to_pitch("E4"),
            note_to_pitch("B3"),
            note_to_pitch("G3"),
            note_to_pitch("D3"),
            note_to_pitch("A2"),
            note_to_pitch("E2")
         );
      }
      elsif( uc($1) eq "DADGBE" )
      {
         @tuning =
         (
            note_to_pitch("E4"),
            note_to_pitch("B3"),
            note_to_pitch("G3"),
            note_to_pitch("D3"),
            note_to_pitch("A2"),
            note_to_pitch("D2")
         );
      }
      else
      {
         die "$.: Unsupported tuning: $1\n";
      }
      output_line($line, $line_number);
   }
   elsif( $line =~ /^\d+[ +]+$/s )
   {
      # Plot note data.
      my $ruler = $line;
      my @tab = ();
      for(my $i = 0; $i < STRING_COUNT; $i++)
      {
         unless( defined($line = <>) )
         {
            die "$.: Incomplete note data\n";
         }
         chomp $line;
         push @tab, $line;
      }
      output_plot($key_signature, \@tuning, $ruler, \@tab);

      # Output original lines.
      output_line($ruler, $line_number);
      for(my $i = 0; $i < STRING_COUNT; $i++)
      {
         output_line($tab[$i] . "\n", $line_number + $i + 1);
      }
      $line_number += STRING_COUNT;
   }
   else
   {
      output_line($line, $line_number);
   }
}

print "</pre></body></html>\n";
