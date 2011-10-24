#include <stdio.h>

#include <cv.h>
#include <highgui.h>


using namespace cv;

IplImage* effect(IplImage * source, int someValue) {

	int height, width, step, channels;

	

	cv::Size theSize = cv::Size(640, 400);
	IplImage * target = cvCreateImage(theSize, source->depth, source->nChannels);
	IplImage * laplace = cvCreateImage(theSize, source->depth, source->nChannels);

	cvResize(source, target, CV_INTER_LINEAR);
	cvCopy(target, laplace);
	//IplImage * foobar = &IplImage(target);
	//*img = foobar;
	

	//cv::Mat lala(foobar);

	uchar* data, *laplaceData;

	height    = target->height;
	width     = target->width;
	step      = target->widthStep;
	channels  = target->nChannels;
	data = (uchar*) target->imageData;
	laplaceData = (uchar*) laplace->imageData;

	
	cv::Size * largeBlur = new cv::Size(15, 15);
	Size * smallBlur = new cv::Size(someValue+1, someValue+1);

	Mat a1 = Mat(target); 
	Mat laplaceMat = Mat(laplace);
	//cv::GaussianBlur( a1, a1, *foo, 0);
	cv::Laplacian(a1, laplaceMat, 8, 1, 10);
	cv::blur(a1, a1, *largeBlur);
	cv::blur(laplaceMat, laplaceMat, *smallBlur);
	printf("Processing a %dx%d image with %d channels, widthStep is %d\n",
			height,width,channels, step);


	for(int y=0; y < height; y++) {
		for(int x=0; x < step; x++) {
			if(laplaceData[y * step + x] > 120) {
				data[y * step + x] = 255;
			}
			//data[y * step + x] = data[y * step + x] ^ laplaceData[y * step + x];
		}
	}

	return target;

}



IplImage * oilFilter(IplImage * source) {
	

}




void showHistogram(IplImage * src, const char* windowName) {
	
	Mat hsv;
    cvtColor(src, hsv, CV_BGR2HSV);

    // let's quantize the hue to 30 levels
    // and the saturation to 32 levels
    int hbins = 30, sbins = 32;
    int histSize[] = {hbins, sbins};
    // hue varies from 0 to 179, see cvtColor
    float hranges[] = { 0, 180 };
    // saturation varies from 0 (black-gray-white) to
    // 255 (pure spectrum color)
    float sranges[] = { 0, 256 };
    const float* ranges[] = { hranges, sranges };
    MatND hist;
    // we compute the histogram from the 0-th and 1-st channels
    int channels[] = {0, 1};

    calcHist( &hsv, 1, channels, Mat(), // do not use mask
        hist, 2, histSize, ranges,
        true, // the histogram is uniform
        false );
    double maxVal=0;
    minMaxLoc(hist, 0, &maxVal, 0, 0);

    int scale = 1;
   	Mat histImg = Mat::zeros(sbins*scale, hbins*10, CV_32F);//CV_8UC3);

    for( int h = 0; h < hbins; h++ )
        for( int s = 0; s < sbins; s++ )
        {
            float binVal = hist.at<float>(h, s);
            int intensity = cvRound(binVal*255/maxVal);
			cv::rectangle( histImg, Point(h*scale, s*scale),
                         Point( (h+1)*scale - 1, (s+1)*scale - 1),
                         Scalar::all(intensity),
                         CV_FILLED );
        }

	IplImage* foo = &IplImage(histImg);
    cvShowImage( windowName, foo );
	
	
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

		img = cvQueryFrame(capture);
		

		img = effect(img, someValue);
		showHistogram(img, "histogram");

		cvShowImage("image display", img);
		
		cvWaitKey( 1 );
	}


	cvReleaseImage(&img);

	return 0;
}


