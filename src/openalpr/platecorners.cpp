/*
 * Copyright (c) 2014 New Designs Unlimited, LLC
 * Opensource Automated License Plate Recognition [http://www.openalpr.com]
 *
 * This file is part of OpenAlpr.
 *
 * OpenAlpr is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License
 * version 3 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "platecorners.h"

using namespace cv;
using namespace std;

PlateCorners::PlateCorners(Mat inputImage, PlateLines* plateLines, PipelineData* pipelineData) :
    tlc(pipelineData)
{
  this->pipelineData = pipelineData;

  if (pipelineData->config->debugPlateCorners)
    cout << "PlateCorners constructor" << endl;

  this->inputImage = inputImage;
  this->plateLines = plateLines;

  this->bestHorizontalScore = 9999999999999;
  this->bestVerticalScore = 9999999999999;


}

PlateCorners::~PlateCorners()
{
}

vector<Point> PlateCorners::findPlateCorners()
{
  if (pipelineData->config->debugPlateCorners)
    cout << "PlateCorners::findPlateCorners" << endl;

  timespec startTime;
  getTime(&startTime);

  int horizontalLines = this->plateLines->horizontalLines.size();
  int verticalLines = this->plateLines->verticalLines.size();

  // layout horizontal lines
  for (int h1 = NO_LINE; h1 < horizontalLines; h1++)
  {
    for (int h2 = NO_LINE; h2 < horizontalLines; h2++)
    {
      if (h1 == h2 && h1 != NO_LINE) continue;

      this->scoreHorizontals(h1, h2);
    }
  }

  // layout vertical lines
  for (int v1 = NO_LINE; v1 < verticalLines; v1++)
  {
    for (int v2 = NO_LINE; v2 < verticalLines; v2++)
    {
      if (v1 == v2 && v1 != NO_LINE) continue;

      this->scoreVerticals(v1, v2);
    }
  }

  if (pipelineData->config->debugPlateCorners)
  {
    cout << "Drawing debug stuff..." << endl;

    Mat imgCorners = Mat(inputImage.size(), inputImage.type());
    inputImage.copyTo(imgCorners);
    
    for (uint linenum = 0; linenum < pipelineData->textLines.size(); linenum++)
    {
      for (int i = 0; i < 4; i++)
        circle(imgCorners, pipelineData->textLines[linenum].textArea[i], 2, Scalar(0, 0, 0));
    }

    line(imgCorners, this->bestTop.p1, this->bestTop.p2, Scalar(255, 0, 0), 1, CV_AA);
    line(imgCorners, this->bestRight.p1, this->bestRight.p2, Scalar(0, 0, 255), 1, CV_AA);
    line(imgCorners, this->bestBottom.p1, this->bestBottom.p2, Scalar(0, 0, 255), 1, CV_AA);
    line(imgCorners, this->bestLeft.p1, this->bestLeft.p2, Scalar(255, 0, 0), 1, CV_AA);

    displayImage(pipelineData->config, "Winning top/bottom Boundaries", imgCorners);
  }

  // Check if a left/right edge has been established.
  if (bestLeft.p1.x == 0 && bestLeft.p1.y == 0 && bestLeft.p2.x == 0 && bestLeft.p2.y == 0)
    confidence = 0;
  else if (bestTop.p1.x == 0 && bestTop.p1.y == 0 && bestTop.p2.x == 0 && bestTop.p2.y == 0)
    confidence = 0;
  else
    confidence = 100;

  vector<Point> corners;
  corners.push_back(bestTop.intersection(bestLeft));
  corners.push_back(bestTop.intersection(bestRight));
  corners.push_back(bestBottom.intersection(bestRight));
  corners.push_back(bestBottom.intersection(bestLeft));

  if (pipelineData->config->debugTiming)
  {
    timespec endTime;
    getTime(&endTime);
    cout << "Plate Corners Time: " << diffclock(startTime, endTime) << "ms." << endl;
  }

  return corners;
}

void PlateCorners::scoreVerticals(int v1, int v2)
{
  float score = 0;	// Lower is better

  LineSegment left;
  LineSegment right;

  
  float charHeightToPlateWidthRatio = pipelineData->config->plateWidthMM / pipelineData->config->charHeightMM;
  float idealPixelWidth = tlc.charHeight *  (charHeightToPlateWidthRatio * 1.03);	// Add 3% so we don't clip any characters

  float confidenceDiff = 0;
  float missingSegmentPenalty = 0;
  
  if (v1 == NO_LINE && v2 == NO_LINE)
  {
    //return;

    left = tlc.centerVerticalLine.getParallelLine(-1 * idealPixelWidth / 2);
    right = tlc.centerVerticalLine.getParallelLine(idealPixelWidth / 2 );

    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_VERTICAL * 2;
    confidenceDiff += 2;
  }
  else if (v1 != NO_LINE && v2 != NO_LINE)
  {
    left = this->plateLines->verticalLines[v1].line;
    right = this->plateLines->verticalLines[v2].line;
    confidenceDiff += (1.0 - this->plateLines->verticalLines[v1].confidence);
    confidenceDiff += (1.0 - this->plateLines->verticalLines[v2].confidence);
  }
  else if (v1 == NO_LINE && v2 != NO_LINE)
  {
    right = this->plateLines->verticalLines[v2].line;
    left = right.getParallelLine(idealPixelWidth);
    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_VERTICAL;
    confidenceDiff += (1.0 - this->plateLines->verticalLines[v2].confidence);
  }
  else if (v1 != NO_LINE && v2 == NO_LINE)
  {
    left = this->plateLines->verticalLines[v1].line;
    right = left.getParallelLine(-1 * idealPixelWidth);
    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_VERTICAL;
    confidenceDiff += (1.0 - this->plateLines->verticalLines[v1].confidence);
  }
  
  score += confidenceDiff * SCORING_LINE_CONFIDENCE_WEIGHT;
  score += missingSegmentPenalty;

  // Make sure that the left and right lines are to the left and right of our text 
  // area
  if (tlc.isLeftOfText(left) < 1 || tlc.isLeftOfText(right) > -1)
    return;

  /////////////////////////////////////////////////////////////////////////
  // Score "Distance from the edge...
  /////////////////////////////////////////////////////////////////////////

  float leftDistanceFromEdge =  abs((float) (left.p1.x + left.p2.x) / 2);
  float rightDistanceFromEdge = abs(this->inputImage.cols - ((float) (right.p1.x + right.p2.x) / 2));

  float distanceFromEdge = leftDistanceFromEdge + rightDistanceFromEdge;
  score += distanceFromEdge * SCORING_VERTICALDISTANCE_FROMEDGE_WEIGHT;

  /////////////////////////////////////////////////////////////////////////
  // Score "Boxiness" of the 4 lines.  How close is it to a parallelogram?
  /////////////////////////////////////////////////////////////////////////

  float verticalAngleDiff = abs(left.angle - right.angle);

  score += (verticalAngleDiff) * SCORING_BOXINESS_WEIGHT;

  /////////////////////////////////////////////////////////////////////////
  // Score angle difference from detected character box
  /////////////////////////////////////////////////////////////////////////
  
  float perpendicularCharAngle = tlc.charAngle - 90;
  float charanglediff = abs(perpendicularCharAngle - left.angle) + abs(perpendicularCharAngle - right.angle);

  score += charanglediff * SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT;
  
  //////////////////////////////////////////////////////////////////////////
  // SCORE the shape wrt character position and height relative to position
  //////////////////////////////////////////////////////////////////////////

  Point leftMidLinePoint = left.closestPointOnSegmentTo(tlc.centerVerticalLine.midpoint());
  Point rightMidLinePoint = right.closestPointOnSegmentTo(tlc.centerVerticalLine.midpoint());

  float plateDistance = abs(idealPixelWidth - distanceBetweenPoints(leftMidLinePoint, rightMidLinePoint));

  score += plateDistance * SCORING_DISTANCE_WEIGHT_VERTICAL;

  if (score < this->bestVerticalScore)
  {
    float scorecomponent;

    if (pipelineData->config->debugPlateCorners)
    {
      cout << "xx xx Score: charHeight " << tlc.charHeight << endl;
      cout << "xx xx Score: idealwidth " << idealPixelWidth << endl;
      cout << "xx xx Score: v1,v2= " << v1 << "," << v2 << endl;
      cout << "xx xx Score: Left= " << left.str() << endl;
      cout << "xx xx Score: Right= " << right.str() << endl;


      cout << "Vertical breakdown Score:" << endl;
      
      cout << " -- Missing Segment Score: " << missingSegmentPenalty << "  -- Weight (1.0)" << endl;
      scorecomponent = missingSegmentPenalty ;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Boxiness Score: " << verticalAngleDiff << "  -- Weight (" << SCORING_BOXINESS_WEIGHT << ")" << endl;
      scorecomponent = verticalAngleDiff * SCORING_BOXINESS_WEIGHT;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Distance From Edge Score: " << distanceFromEdge << " -- Weight (" << SCORING_VERTICALDISTANCE_FROMEDGE_WEIGHT << ")" << endl;
      scorecomponent = distanceFromEdge * SCORING_VERTICALDISTANCE_FROMEDGE_WEIGHT;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Distance Score: " << plateDistance << "  -- Weight (" << SCORING_DISTANCE_WEIGHT_VERTICAL << ")" << endl;
      scorecomponent = plateDistance * SCORING_DISTANCE_WEIGHT_VERTICAL;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Char angle Score: " << charanglediff << "  -- Weight (" << SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT << ")" << endl;
      scorecomponent = charanglediff * SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT;
      cout << " -- -- Score:         " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Plate line confidence Score: " << confidenceDiff << "  -- Weight (" << SCORING_LINE_CONFIDENCE_WEIGHT << ")" << endl;
      scorecomponent = confidenceDiff * SCORING_LINE_CONFIDENCE_WEIGHT;
      cout << " -- -- Score:         " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Score: " << score << endl;
    }

    this->bestVerticalScore = score;
    bestLeft = LineSegment(left.p1.x, left.p1.y, left.p2.x, left.p2.y);
    bestRight = LineSegment(right.p1.x, right.p1.y, right.p2.x, right.p2.y);
  }
}
// Score a collection of lines as a possible license plate region.
// If any segments are missing, extrapolate the missing pieces
void PlateCorners::scoreHorizontals(int h1, int h2)
{
  //if (this->debug)
  //    cout << "PlateCorners::scorePlate" << endl;

  float score = 0;	// Lower is better

  LineSegment top;
  LineSegment bottom;

  float charHeightToPlateHeightRatio = pipelineData->config->plateHeightMM / pipelineData->config->charHeightMM;
  float idealPixelHeight = tlc.charHeight *  charHeightToPlateHeightRatio;

  float confidenceDiff = 0;
  float missingSegmentPenalty = 0;
  
  if (h1 == NO_LINE && h2 == NO_LINE)
  {
//    return;


    top = tlc.centerHorizontalLine.getParallelLine(idealPixelHeight / 2);
    bottom = tlc.centerHorizontalLine.getParallelLine(-1 * idealPixelHeight / 2 );

    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_HORIZONTAL * 2;
    confidenceDiff += 2;
  }
  else if (h1 != NO_LINE && h2 != NO_LINE)
  {
    top = this->plateLines->horizontalLines[h1].line;
    bottom = this->plateLines->horizontalLines[h2].line;
    confidenceDiff += (1.0 - this->plateLines->horizontalLines[h1].confidence);
    confidenceDiff += (1.0 - this->plateLines->horizontalLines[h2].confidence);
  }
  else if (h1 == NO_LINE && h2 != NO_LINE)
  {
    bottom = this->plateLines->horizontalLines[h2].line;
    top = bottom.getParallelLine(idealPixelHeight);
    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_HORIZONTAL;
    confidenceDiff += (1.0 - this->plateLines->horizontalLines[h2].confidence);
  }
  else if (h1 != NO_LINE && h2 == NO_LINE)
  {
    top = this->plateLines->horizontalLines[h1].line;
    bottom = top.getParallelLine(-1 * idealPixelHeight);
    missingSegmentPenalty += SCORING_MISSING_SEGMENT_PENALTY_HORIZONTAL;
    confidenceDiff += (1.0 - this->plateLines->horizontalLines[h1].confidence);
  }
  
  score += confidenceDiff * SCORING_LINE_CONFIDENCE_WEIGHT;
  score += missingSegmentPenalty;

  // Make sure that the top and bottom lines are above and below
  // the text area
  if (tlc.isAboveText(top) < 1 || tlc.isAboveText(bottom) > -1)
    return;
  
  // We now have 4 possible lines.  Let's put them to the test and score them...

  /////////////////////////////////////////////////////////////////////////
  // Score "Boxiness" of the 4 lines.  How close is it to a parallelogram?
  /////////////////////////////////////////////////////////////////////////

  float horizontalAngleDiff = abs(top.angle - bottom.angle);

  score += (horizontalAngleDiff) * SCORING_BOXINESS_WEIGHT;
//  if (this->debug)
//    cout << "PlateCorners boxiness score: " << (horizontalAngleDiff + verticalAngleDiff) * SCORING_BOXINESS_WEIGHT << endl;

  //////////////////////////////////////////////////////////////////////////
  // SCORE the shape wrt character position and height relative to position
  //////////////////////////////////////////////////////////////////////////

  Point topPoint = top.midpoint();
  Point botPoint = bottom.closestPointOnSegmentTo(topPoint);
  float plateHeightPx = distanceBetweenPoints(topPoint, botPoint);

  // Get the height difference

  float heightRatio = tlc.charHeight / plateHeightPx;
  float idealHeightRatio = (pipelineData->config->charHeightMM / pipelineData->config->plateHeightMM);
  //if (leftRatio < MIN_CHAR_HEIGHT_RATIO || leftRatio > MAX_CHAR_HEIGHT_RATIO || rightRatio < MIN_CHAR_HEIGHT_RATIO || rightRatio > MAX_CHAR_HEIGHT_RATIO)
  float heightRatioDiff = abs(heightRatio - idealHeightRatio);
  // Ideal ratio == ~.45

  // Get the distance from the top and the distance from the bottom
  // Take the average distances from the corners of the character region to the top/bottom lines
//  float topDistance  = distanceBetweenPoints(topMidLinePoint, charRegion->getCharBoxTop().midpoint());
//  float bottomDistance = distanceBetweenPoints(bottomMidLinePoint, charRegion->getCharBoxBottom().midpoint());

//  float idealTopDistance = charHeight * (TOP_WHITESPACE_HEIGHT_MM / CHARACTER_HEIGHT_MM);
//  float idealBottomDistance = charHeight * (BOTTOM_WHITESPACE_HEIGHT_MM / CHARACTER_HEIGHT_MM);
//  float distScore = abs(topDistance - idealTopDistance) + abs(bottomDistance - idealBottomDistance);

  score += heightRatioDiff * SCORING_PLATEHEIGHT_WEIGHT;

  //////////////////////////////////////////////////////////////////////////
  // SCORE the middliness of the stuff.  We want our top and bottom line to have the characters right towards the middle
  //////////////////////////////////////////////////////////////////////////

  Point charAreaMidPoint = tlc.centerVerticalLine.midpoint();
  Point topLineSpot = top.closestPointOnSegmentTo(charAreaMidPoint);
  Point botLineSpot = bottom.closestPointOnSegmentTo(charAreaMidPoint);

  float topDistanceFromMiddle = distanceBetweenPoints(topLineSpot, charAreaMidPoint);
  float bottomDistanceFromMiddle = distanceBetweenPoints(botLineSpot, charAreaMidPoint);

  float idealDistanceFromMiddle = idealPixelHeight / 2;

  float middleScore = abs(topDistanceFromMiddle - idealDistanceFromMiddle) + abs(bottomDistanceFromMiddle - idealDistanceFromMiddle);

  score += middleScore * SCORING_TOP_BOTTOM_SPACE_VS_CHARHEIGHT_WEIGHT;

//  if (this->debug)
//  {
//    cout << "PlateCorners boxiness score: " << avgRatio * SCORING_TOP_BOTTOM_SPACE_VS_CHARHEIGHT_WEIGHT << endl;
//    cout << "PlateCorners boxiness score: " << distScore * SCORING_PLATEHEIGHT_WEIGHT << endl;
//  }
  //////////////////////////////////////////////////////////////
  // SCORE: the shape for angles matching the character region
  //////////////////////////////////////////////////////////////

  float charanglediff = abs(tlc.charAngle - top.angle) + abs(tlc.charAngle - bottom.angle);

  score += charanglediff * SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT;

//  if (this->debug)
//    cout << "PlateCorners boxiness score: " << charanglediff * SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT << endl;

  if (score < this->bestHorizontalScore)
  {
    float scorecomponent;

    if (pipelineData->config->debugPlateCorners)
    {
      cout << "xx xx Score: charHeight " << tlc.charHeight << endl;
      cout << "xx xx Score: idealHeight " << idealPixelHeight << endl;
      cout << "xx xx Score: h1,h2= " << h1 << "," << h2 << endl;
      cout << "xx xx Score: Top= " << top.str() << endl;
      cout << "xx xx Score: Bottom= " << bottom.str() << endl;

      
      cout << "Horizontal breakdown Score:" << endl;
      
      cout << " -- Missing Segment Score: " << missingSegmentPenalty << "  -- Weight (1.0)" << endl;
      scorecomponent = missingSegmentPenalty ;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Boxiness Score: " << horizontalAngleDiff << "  -- Weight (" << SCORING_BOXINESS_WEIGHT << ")" << endl;
      scorecomponent = horizontalAngleDiff * SCORING_BOXINESS_WEIGHT;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Height Ratio Diff Score: " << heightRatioDiff << "  -- Weight (" << SCORING_PLATEHEIGHT_WEIGHT << ")" << endl;
      scorecomponent = heightRatioDiff * SCORING_PLATEHEIGHT_WEIGHT;
      cout << " -- -- " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Distance Score: " << middleScore << "  -- Weight (" << SCORING_TOP_BOTTOM_SPACE_VS_CHARHEIGHT_WEIGHT << ")" << endl;
      scorecomponent = middleScore * SCORING_TOP_BOTTOM_SPACE_VS_CHARHEIGHT_WEIGHT;
      cout << " -- -- Score:       " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Char angle Score: " << charanglediff << "  -- Weight (" << SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT << ")" << endl;
      scorecomponent = charanglediff * SCORING_ANGLE_MATCHES_LPCHARS_WEIGHT;
      cout << " -- -- Score:         " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;

      cout << " -- Plate line confidence Score: " << confidenceDiff << "  -- Weight (" << SCORING_LINE_CONFIDENCE_WEIGHT << ")" << endl;
      scorecomponent = confidenceDiff * SCORING_LINE_CONFIDENCE_WEIGHT;
      cout << " -- -- Score:         " << scorecomponent << " = " << scorecomponent / score * 100 << "% of score" << endl;
      
      cout << " -- Score: " << score << endl;
    }
    this->bestHorizontalScore = score;
    bestTop = LineSegment(top.p1.x, top.p1.y, top.p2.x, top.p2.y);
    bestBottom = LineSegment(bottom.p1.x, bottom.p1.y, bottom.p2.x, bottom.p2.y);
  }
}

TextLineCollection::TextLineCollection(PipelineData* pipelineData) {
  
  this->pipelineData = pipelineData;
  
  charHeight = 0;
  charAngle = 0;
  for (uint i = 0; i < pipelineData->textLines.size(); i++)
  {
    charHeight += pipelineData->textLines[i].lineHeight;
    charAngle += pipelineData->textLines[i].angle;
    
  }
  charHeight = charHeight / pipelineData->textLines.size();
  charAngle = charAngle / pipelineData->textLines.size();
  
  this->topCharArea = pipelineData->textLines[0].charBoxTop;
  this->bottomCharArea = pipelineData->textLines[0].charBoxBottom;
  for (uint i = 1; i < pipelineData->textLines.size(); i++)
  {
    
    if (this->topCharArea.isPointBelowLine(pipelineData->textLines[i].charBoxTop.midpoint()) == false)
      this->topCharArea = pipelineData->textLines[i].charBoxTop;
    
    if (this->bottomCharArea.isPointBelowLine(pipelineData->textLines[i].charBoxBottom.midpoint()))
      this->bottomCharArea = pipelineData->textLines[i].charBoxBottom;

  }
  
  longerSegment = this->bottomCharArea;
  shorterSegment = this->topCharArea;
  if (this->topCharArea.length > this->bottomCharArea.length)
  {
    longerSegment = this->topCharArea;
    shorterSegment = this->bottomCharArea;
  }
  
  findCenterHorizontal();
  findCenterVertical();
  // Center Vertical Line
  
  if (pipelineData->config->debugPlateCorners)
  {
    Mat debugImage = Mat::zeros(pipelineData->crop_gray.size(), CV_8U);
    line(debugImage, this->centerHorizontalLine.p1, this->centerHorizontalLine.p2, Scalar(255,255,255), 2);
    line(debugImage, this->centerVerticalLine.p1, this->centerVerticalLine.p2, Scalar(255,255,255), 2);

    displayImage(pipelineData->config, "Plate Corner Center lines", debugImage);
  }
}

// Returns 1 for above, 0 for within, and -1 for below
int TextLineCollection::isAboveText(LineSegment line) {
  // Test four points (left and right corner of top and bottom line)
  
  Point topLeft = line.closestPointOnSegmentTo(topCharArea.p1);
  Point topRight = line.closestPointOnSegmentTo(topCharArea.p2);
  
  bool lineIsBelowTop = topCharArea.isPointBelowLine(topLeft) || topCharArea.isPointBelowLine(topRight);
  
  if (!lineIsBelowTop)
    return 1;
  
  Point bottomLeft = line.closestPointOnSegmentTo(bottomCharArea.p1);
  Point bottomRight = line.closestPointOnSegmentTo(bottomCharArea.p2);
  
  bool lineIsBelowBottom = bottomCharArea.isPointBelowLine(bottomLeft) &&
                          bottomCharArea.isPointBelowLine(bottomRight);
  
  if (lineIsBelowBottom)
    return -1;
  
  return 0;
  
}

// Returns 1 for left, 0 for within, and -1 for to the right
int TextLineCollection::isLeftOfText(LineSegment line) {

  LineSegment leftSide = LineSegment(bottomCharArea.p1, topCharArea.p1);
  
  Point topLeft = line.closestPointOnSegmentTo(leftSide.p2);
  Point bottomLeft = line.closestPointOnSegmentTo(leftSide.p1);
  
  bool lineIsAboveLeft = (!leftSide.isPointBelowLine(topLeft)) && (!leftSide.isPointBelowLine(bottomLeft));
  
  if (lineIsAboveLeft)
    return 1;
  
  LineSegment rightSide = LineSegment(bottomCharArea.p2, topCharArea.p2);
  
  Point topRight = line.closestPointOnSegmentTo(rightSide.p2);
  Point bottomRight = line.closestPointOnSegmentTo(rightSide.p1);
  
  
  bool lineIsBelowRight = rightSide.isPointBelowLine(topRight) && rightSide.isPointBelowLine(bottomRight);
  
  if (lineIsBelowRight)
    return -1;
  
  return 0;
}

void TextLineCollection::findCenterHorizontal() {
  // To find the center horizontal line:
  // Find the longer of the lines (if multiline)
  // Get the nearest point on the bottom-most line for the 
  // left and right 
  

  
  Point leftP1 =  shorterSegment.closestPointOnSegmentTo(longerSegment.p1);
  Point leftP2 = longerSegment.p1;
  LineSegment left = LineSegment(leftP1, leftP2);
  
  Point leftMidpoint = left.midpoint();
  
  
  
  Point rightP1 =  shorterSegment.closestPointOnSegmentTo(longerSegment.p2);
  Point rightP2 = longerSegment.p2;
  LineSegment right = LineSegment(rightP1, rightP2);
  
  Point rightMidpoint = right.midpoint();
  
  this->centerHorizontalLine = LineSegment(leftMidpoint, rightMidpoint);
  
}

void TextLineCollection::findCenterVertical() {
  // To find the center vertical line:
  // Choose the longest line (if multiline)
  // Get the midpoint
  // Draw a line up/down using the closest point on the bottom line
  

  Point p1 = longerSegment.midpoint();
  
  Point p2 = shorterSegment.closestPointOnSegmentTo(p1);
  
  // Draw bottom to top
  if (p1.y < p2.y)
    this->centerVerticalLine = LineSegment(p1, p2);
  else
    this->centerVerticalLine = LineSegment(p2, p1);
}



