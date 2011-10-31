#include <stdio.h>

#include <cv.h>
#include <highgui.h>

/*
 * Performs an in-place resizing (that, is the old image is destroyed!) of the passed
 * image to the specified width and height.
 */
IplImage* resizeInPlace(IplImage** image, int width, int height, bool releaseSourceImage = true) {
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



IplImage * oilFilter(IplImage * source) {


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

	//double max_value = 0;
	//minMaxLoc(histogram, 0, &max_value, 0, 0);
	//printf("only max: %f\n", max_value);


}





int main(void) {
	IplImage* img = cvLoadImage("./Data/background.jpg");

	CvCapture *capture = 0;
	/* initialize camera */
    capture = cvCaptureFromCAM( 0 );
	
    if ( !capture ) {
        fprintf( stderr, "Cannot open initialize webcam!\n" );
        return 1;
    }
	
	//img = cvQueryFrame( capture );

	//data      = (uchar *) img->imageData;



//	for(int x=0; x < step/3; x++)
//		data[100 * step + (x*3)] = ;



	//cv::Mat m = cv::Mat(img, true);
	/*IplImage img2 = m;
	img2.height = height;
	img2.width = width;
	img2.widthStep = step;

	for(int y=0; y < height; y++) {
		for(int x=0; x < width; x++) {
			int ownStep = (width * channels);
			ownStep += step - ownStep;
			img2.imageData[y * step + x] = data[y * ownStep + x];
		}
	}*/
	

	//img2.imageData[0] = 0xFF;
	//img2.imageData[1] = 0xFF;
	//img2.imageData[2] = 0xFF;

	cvNamedWindow("image display", CV_WINDOW_AUTOSIZE);
	cvNamedWindow("histogram", CV_WINDOW_AUTOSIZE);
	//cvMoveWindow("image display", 0, 0);
	
	int someValue = 0;
	cvCreateTrackbar( "some value", "image display", &someValue, 100, NULL );
	//cvWaitKey(0);
	
	while(1) {

		IplImage* img = cvQueryFrame(capture);

		resizeInPlace(&img, 640, 360, false);
		img = effect(img, someValue);
		//showHistogram(img, "histogram");
		cv::MatND histRed;
		cv::MatND histGreen;
		cv::MatND histBlue;

		calcHistogramAdv(histRed, histGreen, histBlue, img);


		IplImage * foo = drawHistogram(&histBlue, 3);
		cvShowImage( "histogram", foo );

		cvShowImage("image display", img);

		cvReleaseImage(&img);

		cvWaitKey( 1 );
	}


	//cvReleaseImage(&img);

	return 0;
}


