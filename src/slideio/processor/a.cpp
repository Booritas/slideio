
const cv::Point& nextMove(const cv::Point& current, const cv::Point& center) {
   static cv::Point[3][3] =
   [
      [{0, -1},{1,-1},{1,0}],
      [{-1,-1},{0,0},{1,1}],
      [{-1,0},{-1,1},{0,1}]
   ];
   cv::Point offset = current - center + cv::Point(1,1);
   return center + nextMove[offset.x][offset.y];
}

void mooreNeighborTracing(const cv::Mat& image, int value) {
   // Define the 8 possible directions for tracing
   static const int directions[8][2] = {
      {-1, 0}, {-1, 1}, {0, 1}, {1, 1},
      {1, 0}, {1, -1}, {0, -1}, {-1, -1}
   };

   // Get the image dimensions
   int rows = image.rows;
   int cols = image.cols;

   // Create a visited matrix to keep track of visited pixels
   cv::Mat visited(rows, cols, CV_8UC1, cv::Scalar(0));

   // Iterate over each pixel in the image
   for (int i = 0; i < rows; i++) {
      for (int j = 0; j < cols; j++) {
         // Check if the current pixel has the specified value
         if (image.at<int>(i, j) == value && visited.at<uchar>(i, j) == 0) {
            // Start tracing from the current pixel
            cv::Point current(j, i);
            cv::Point start = current;
            cv::Point previous = current;
            cv::Point next;

            // Trace the boundary
            do {
               // Mark the current pixel as visited
               visited.at<uchar>(current.y, current.x) = 255;

               // Find the next pixel in the boundary
               for (int k = 0; k < 8; k++) {
                  next = current + cv::Point(directions[k][1], directions[k][0]);
                  if (next.x >= 0 && next.x < cols && next.y >= 0 && next.y < rows &&
                     image.at<int>(next.y, next.x) == value && visited.at<uchar>(next.y, next.x) == 0) {
                     break;
                  }
               }

               // Update the current and previous pixels
               previous = current;
               current = next;

               // Do something with the boundary pixel (e.g., store it, draw it, etc.)
               // ...

            } while (current != start);
         }
      }
   }
}