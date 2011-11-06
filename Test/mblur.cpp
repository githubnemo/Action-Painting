#include <stdio.h>

#include <cv.h>
#include <highgui.h>


inline uchar getPixel(uchar* img, int step, int x, int y) {
	return img[y*step + x];
}


typedef struct rgb {
	uchar r, g, b;
} rgb;


rgb applyMask(
		const uchar* mask,
		uchar* masked,
		uchar* img,
		int x,
		int y,
		int w,
		int h,
		int step,
		int imgheight)
{
	int my = y - (h/2); // Mask left top corner position
	int mx = x - (w/2);	// is at (my,mx).
	int rsum = 0;
	int gsum = 0;
	int bsum = 0;

	for(int i=0; i < h; i++) { // Iterate over mask
		for(int j=0; j < w; j++) {
			int rvalue = 0;
			int gvalue = 0;
			int bvalue = 0;

			if(mx+(j*3) >= 0 && mx+(j*3) < step && my+i < imgheight ) {
				gvalue = getPixel(img, step, (mx+(j*3)+0), my+i);
				rvalue = getPixel(img, step, (mx+(j*3)+1), my+i);
				bvalue = getPixel(img, step, (mx+(j*3)+2), my+i);
			}

			rsum += mask[i*w + j] * rvalue;
			gsum += mask[i*w + j] * gvalue;
			bsum += mask[i*w + j] * bvalue;
		}
	}

	return (rgb){rsum/(w*h),gsum/(w*h),bsum/(w*h)};
}


int main(void) {
	IplImage* img = cvLoadImage("background.jpg");
	int height, width, step, channels;
	uchar* data;

	height    = img->height;
	width     = img->width;
	step      = img->widthStep;
	channels  = img->nChannels;
	data      = (uchar *)img->imageData;
	printf("Processing a %dx%d image with %d channels, widthStep is %d\n",
			height,width,channels, step);


	/*
	 * direction is south-east
	 *
	 * 1  1  1
	 * 1  1  2
	 * 1  2  4
	 *
	 * => 1 1 1     1 1 1 ^ direction
	 *    1 1 1  *  1 1 2
	 *    1 1 1     1 2 4
	 *
	 *
	 *   \   \       Apply mask as seen on the left.
	 *    \   \
	 *     \   \
	 *      \   \
	 */


	/*
	const uchar mask[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 2, 2, 2, 2, 2,
						   1, 1, 1, 1, 2, 2, 2, 3, 3,
						   1, 1, 1, 1, 2, 2, 2, 3, 3,
						   1, 1, 1, 1, 2, 2, 3, 3, 3,
						   1, 1, 1, 1, 2, 2, 3, 3, 4,};
	*/

	int maskWidth = 5;
	int maskHeight = 5;

#if 0
	const uchar mask[] = { 1, 1, 1,
						   1, 1, 1,
						   1, 1, 1 };
#endif

#if 1
	const uchar mask[] = { 0, 0, 1, 1, 1,
						   0, 0, 1, 1, 1,
						   1, 1, 0, 1, 1,
						   1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1};
#endif

#if 0
	const uchar mask[] = { 0, 0, 1, 0, 0,
						   0, 1, 1, 1, 0,
						   1, 1, 1, 1, 1,
						   0, 1, 1, 1, 0,
						   0, 0, 1, 0, 0};
#endif

	uchar* masked = new uchar[maskWidth*maskHeight];
	rgb v = {0,0,0};

	uchar* imgcopy = new uchar[height*step];
	memcpy(imgcopy, img->imageData, height*step);

	for(int y=0; y < height; y++) {
		for(int x=0; x < step; x+=3) {
			v = applyMask(mask, masked, imgcopy, x, y, maskWidth, maskHeight, step, height);

			img->imageData[y * step + x+0] = v.b;
			img->imageData[y * step + x+1] = v.g;
			img->imageData[y * step + x+2] = v.r;
		}
	}



	cvNamedWindow("image display", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("image display", 100, 100);

	cvShowImage("image display", img);

	cvWaitKey(0);

	cvReleaseImage(&img);

	return 0;
}
