#include"util.h"
#include"common.h"

// Return an angle value that is halfway between current and target.
int ConvergeAngle(int current_angle, int target_angle, float fraction)
{
   assert(current_angle >= 0);
   assert(current_angle < 360);
   assert(target_angle >= 0);
   assert(target_angle < 360);
   assert(fraction >= 0);
   assert(fraction <= 1);

   if( current_angle == target_angle )
      return target_angle;

   int clockwise_delta, counterclockwise_delta;
   if( current_angle <= target_angle )
   {
      clockwise_delta = target_angle - current_angle;
      counterclockwise_delta = current_angle + 360 - target_angle;
   }
   else
   {
      clockwise_delta = target_angle + 360 - current_angle;
      counterclockwise_delta = current_angle - target_angle;
   }
   assert(clockwise_delta >= 0);
   assert(clockwise_delta <= 360);
   assert(counterclockwise_delta >= 0);
   assert(counterclockwise_delta <= 360);

   if( clockwise_delta < counterclockwise_delta )
      return (current_angle + (int)(fraction * clockwise_delta)) % 360;
   return (current_angle + 360 - (int)(fraction * counterclockwise_delta)) % 360;
}
