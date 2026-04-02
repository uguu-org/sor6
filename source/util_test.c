#include<assert.h>
#include<stdlib.h>
#include"util.h"

int main(void)
{
   // Check same angles.
   for(int i = 0; i < 360; i++)
      assert(ConvergeAngle(i, i, 0.5f) == i);

   // Check clockwise rotation.
   for(int i = 0; i < 360; i++)
   {
      for(int j = i; j < i + 180; j++)
      {
         const int expected = ((i + j) / 2) % 360;
         const int actual = ConvergeAngle(i, j % 360, 0.5f);
         assert(abs(expected - actual) <= 1);
      }
   }

   // Check counterclockwise rotation.
   for(int i = 0; i < 360; i++)
   {
      for(int j = i - 179; j < i; j++)
      {
         const int expected = ((i + j) / 2 + 360) % 360;
         const int actual = ConvergeAngle(i, (j + 360) % 360, 0.5f);
         assert(abs(expected - actual) <= 1);
      }
   }

   // Check fraction multiplier.
   for(int i = 0; i < 180; i++)
   {
      assert(ConvergeAngle(i, i + 80, 0.0f) == i);
      assert(ConvergeAngle(i, i + 80, 0.25f) == i + 20);
      assert(ConvergeAngle(i, i + 80, 0.5f) == i + 40);
      assert(ConvergeAngle(i, i + 80, 0.75f) == i + 60);
      assert(ConvergeAngle(i, i + 80, 1.0f) == i + 80);
   }
   for(int i = 0; i < 180; i++)
   {
      assert(ConvergeAngle(i + 80, i, 0.0f) == i + 80);
      assert(ConvergeAngle(i + 80, i, 0.25f) == i + 60);
      assert(ConvergeAngle(i + 80, i, 0.5f) == i + 40);
      assert(ConvergeAngle(i + 80, i, 0.75f) == i + 20);
      assert(ConvergeAngle(i + 80, i, 1.0f) == i);
   }

   return 0;
}
