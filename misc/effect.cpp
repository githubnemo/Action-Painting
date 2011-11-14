#include <stdio.h>

#include <cv.h>
#include <highgui.h>

/*
 * Performs an in-place resizing (that, is the old image is destroyed!) of the passed
 * image to the specified width and height.
 */
void resizeInPlace(IplImage** image, int width, int height, bool releaseSourceImage = true) {
	assert(width > 0 && height > 0);

	IplImage* source = *image;

	cv::Size targetSize = cv::Size(width, height);
	IplImage* target = cvCreateImage(targetSize, source->depth, source->nChannels);

	cvResize(source, target, CV_INTER_LINEAR);

	if(releaseSourceImage) {
		cvReleaseImage(&source);
	}

	*image = target;
}


IplImage* effect(IplImage * source, int someValue) {

	int height, width, step, channels;

	cv::Size targetSize = cv::Size(640, 400);

	IplImage * target = cvCreateImage(targetSize, source->depth, source->nChannels);
	IplImage * laplace = cvCreateImage(targetSize, source->depth, source->nChannels);

	cvResize(source, target, CV_INTER_LINEAR);
	cvCopy(target, laplace);

	uchar* data, *laplaceData;

	height    = target->height;
	width     = target->width;
	step      = target->widthStep;
	channels  = target->nChannels;

	data = (uchar*) target->imageData;
	laplaceData = (uchar*) laplace->imageData;


	cv::Size * largeBlur = new cv::Size(15, 15);
	cv::Size * smallBlur = new cv::Size(someValue+1, someValue+1);

	cv::Mat targetMat = cv::Mat(target);
	cv::Mat laplaceMat = cv::Mat(laplace);

	cv::Laplacian(targetMat, laplaceMat, 8, 1, 10);
	cv::blur(targetMat, targetMat, *largeBlur); // In place blur

	cv::blur(laplaceMat, laplaceMat, *smallBlur);

	/*
     printf("Processing a %dx%d image with %d channels, widthStep is %d\n",
     height,width,channels, step);
     /**/

	int laplaceThreshold = 120;

	for(int y=0; y < height; y++) {
		for(int x=0; x < step; x++) {
			if(laplaceData[y * step + x] > laplaceThreshold) {
				data[y * step + x] = 255;
			}
		}
	}

	return target;
}



IplImage * drawHistogram(cv::MatND * histogramPointer, int scale) {
	double histMax = 0;


	cv::MatND histogram = *histogramPointer;
	minMaxLoc(histogram, 0, &histMax, 0, 0);

	int scaleX, scaleY;
	scaleX = scaleY = scale;

	IplImage* imgHist = cvCreateImage(cvSize(256*scaleX, 64*scaleY), 8 ,1);
    cvZero(imgHist);

    for(int i=0;i<255;i++)
    {
        float histValue = histogram.at<float>(i);
        float nextValue = histogram.at<float>(i+1);

        CvPoint pt1 = cvPoint(i*scaleX, 64*scaleY);
        CvPoint pt2 = cvPoint(i*scaleX+scaleX, 64*scaleY);
        CvPoint pt3 = cvPoint(i*scaleX+scaleX, (64-nextValue*64/histMax)*scaleY);
        CvPoint pt4 = cvPoint(i*scaleX, (64-histValue*64/histMax)*scaleY);

        int numPts = 5;
        CvPoint pts[] = {pt1, pt2, pt3, pt4, pt1};

        cvFillConvexPoly(imgHist, pts, numPts, cvScalar(127));
    }

	return imgHist;
}

void calcLeveledHistogram(IplImage* img, int levels,
                          cv::MatND& histRed,
                          cv::MatND& histGreen,
                          cv::MatND& histBlue) {

    IplImage* imgRed = cvCreateImage(cvGetSize(img), 8, 1);
    IplImage* imgGreen = cvCreateImage(cvGetSize(img), 8, 1);
    IplImage* imgBlue = cvCreateImage(cvGetSize(img), 8, 1);

    cvSplit(img, imgBlue, imgGreen, imgRed, NULL);

	cv::Mat red = cv::Mat(imgRed);
	cv::Mat green = cv::Mat(imgGreen);
	cv::Mat blue = cv::Mat(imgBlue);

    red = cv::Scalar(0);
    green = cv::Scalar(0);
    blue = cv::Scalar(0);

    int width = img->width;
    int height = img->height;



    for(int y=0; y<height; y++) {
        for(int x=0; x<width; x++) {

            uchar r = red.at<uchar>(y,x);
            uchar g = green.at<uchar>(y,x);
            uchar b = blue.at<uchar>(y,x);

            int ri = r * levels / 256;
            int gi = g * levels / 256;
            int bi = b * levels / 256;

            float histR = histRed.at<float>(r);
            float histG = histGreen.at<float>(g);
            float histB = histBlue.at<float>(b);

            histRed.at<float>(r) += 1.f;
            histGreen.at<float>(g) += 1.f;
            histBlue.at<float>(b) += 1.f;


        }
    }



}

void calcHistogramAdv(cv::MatND& histRed, cv::MatND& histGreen, cv::MatND& histBlue, IplImage * img) {

	IplImage* imgRed = cvCreateImage(cvGetSize(img), 8, 1);
    IplImage* imgGreen = cvCreateImage(cvGetSize(img), 8, 1);
    IplImage* imgBlue = cvCreateImage(cvGetSize(img), 8, 1);

    cvSplit(img, imgBlue, imgGreen, imgRed, NULL);

	cv::Mat red = cv::Mat(imgRed);
	cv::Mat green = cv::Mat(imgGreen);
	cv::Mat blue = cv::Mat(imgBlue);

	int bins = 256;
	int images = 1;
	int channels = 0;
	int dimensions = 1;
	int histSize[] = { bins };

	float rgbrange[] = {0, 256};
	const float * ranges[] = { rgbrange };
	cv::Mat mask = cv::Mat();


	cv::calcHist(
                 &red,
                 images,
                 &channels,
                 mask,
                 histRed,
                 dimensions,
                 histSize,
                 ranges
                 );


	cv::calcHist(
                 &green,
                 images,
                 &channels,
                 mask,
                 histGreen,
                 dimensions,
                 histSize,
                 ranges
                 );


	cv::calcHist(
                 &blue,
                 images,
                 &channels,
                 mask,
                 histBlue,
                 dimensions,
                 histSize,
                 ranges
                 );

	cvReleaseImage(&imgRed);
	cvReleaseImage(&imgGreen);
	cvReleaseImage(&imgBlue);

	//double max_value = 0;
	//minMaxLoc(histogram, 0, &max_value, 0, 0);
	//printf("only max: %f\n", max_value);


}





int main(void) {

	CvCapture *capture = 0;
	/* initialize camera */
    capture = cvCaptureFromCAM( 0 );

    if ( !capture ) {
        fprintf( stderr, "Cannot open initialize webcam!\n" );
        return 1;
    }


	cvNamedWindow("image display", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("histogram", CV_WINDOW_AUTOSIZE);
	//cvMoveWindow("image display", 0, 0);

	int someValue = 0;
	cvCreateTrackbar( "some value", "image display", &someValue, 100, NULL );
	//cvWaitKey(0);


	int i=0;
	while(1) {
		//i++;
		IplImage* img = cvQueryFrame(capture);

		resizeInPlace(&img, 640, 360, false);
        //		img = effect(img, someValue);
		//showHistogram(img, "histogram");
		cv::MatND histRed;
		cv::MatND histGreen;
		cv::MatND histBlue;

		calcHistogramAdv(histRed, histGreen, histBlue, img);

        /*const int histSize[] = {256};
         cv::MatND histRedA(1, histSize, CV_32F);
         cv::MatND histGreenA(1, histSize, CV_32F);
         cv::MatND histBlueA(1, histSize, CV_32F);
         calcLeveledHistogram(img, 256, histRedA, histGreenA, histBlueA);


         double max_value = 0;
         minMaxLoc(histRedA, 0, &max_value, 0, 0);
         printf("only max: %f\n", max_value);

         IplImage * foo = drawHistogram(&histRedA, 3);
         cvShowImage( "histogram", foo );
         cvReleaseImage(&foo);

         */


        cv::Mat imgMat(img);

        IplImage *dst = cvCreateImage( cvSize( 640, 360 ),
                                      img->depth, img->nChannels );
        cv::Mat dstMat(dst);
        cvCopy(img, dst);
        cv::medianBlur(imgMat, dstMat, someValue | 1);

		cvShowImage("image display", dst);

		// Clean up
		cvReleaseImage(&img);
		histRed.release();
		histGreen.release();
		histBlue.release();

		cvWaitKey( 1 );
	}


	//cvReleaseImage(&img);

	return 0;
}
