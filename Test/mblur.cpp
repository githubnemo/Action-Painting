#include <stdio.h>

#include <cv.h>
#include <highgui.h>


inline uchar getPixel(uchar* img, int step, int x, int y) {
	return img[y*step + x];
}


typedef struct rgb {
	uchar r, g, b;
} rgb;

typedef struct mask_s {
	const uchar* mask;
	int width;
	int height;
} mask_t;



void getMaskPixels(const mask_t& mask, uchar* masked, uchar* img, int step, int height, int x, int y) {
	int my = y - (mask.height/2);	// Mask left top corner position
	int mx = x - (mask.width/2);	// is at (my,mx).

	int h = mask.height;
	int w = mask.width;

	// Get pixel to mask
	for(int i=0; i < h; i++) {
		for(int j=0; j < w; j++) {
			int rvalue = 0;
			int gvalue = 0;
			int bvalue = 0;

			if(mask.mask[i*mask.height + j]
			&& mx+(j*3) >= 0 && mx+(j*3) < step && my+i < height ) {
				bvalue = getPixel(img, step, (mx+(j*3)+0), my+i);
				gvalue = getPixel(img, step, (mx+(j*3)+1), my+i);
				rvalue = getPixel(img, step, (mx+(j*3)+2), my+i);
			}

			masked[i*mask.width + j + 0] = bvalue;
			masked[i*mask.width + j + 1] = gvalue;
			masked[i*mask.width + j + 2] = rvalue;
		}
	}
}

void applyMotionMask(
		const mask_t& mask,
		uchar* masked1,
		uchar* masked2,
		uchar* simg,
		uchar* timg,
		int step,
		int height,
		int x1,	// from
		int y1,
		int x2,	// to
		int y2)
{
	getMaskPixels(mask, masked1, simg, step, height, x1, y1);
	getMaskPixels(mask, masked2, simg, step, height, x2, y2);

	int h = mask.height;
	int w = mask.width;

	for(int i=0; i < h; i++) {
		for(int j=0; j < w; j++) {
			int lpos = i*w+j;

			int bvalue = (masked1[lpos + 0] + masked2[lpos + 0]) / 2;
			int gvalue = (masked1[lpos + 1] + masked2[lpos + 1]) / 2;
			int rvalue = (masked1[lpos + 2] + masked2[lpos + 2]) / 2;

			timg[(y2+i)*step + x2+(j*3) + 0] = bvalue;
			timg[(y2+i)*step + x2+(j*3) + 1] = gvalue;
			timg[(y2+i)*step + x2+(j*3) + 2] = rvalue;
		}
	}
}





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


#if 1
	const uchar mask[] = { 1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1, 1, 1, 1, 1,};
#endif

	int maskWidth = 9;
	int maskHeight = 9;

#if 0
	const uchar mask[] = { 1, 1, 1,
						   1, 1, 1,
						   1, 1, 1 };
#endif

#if 0
	const uchar mask[] = { 1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1,
						   1, 1, 1, 1, 1,
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

	uchar* masked1 = new uchar[maskWidth*maskHeight*3];
	uchar* masked2 = new uchar[maskWidth*maskHeight*3];
	rgb v = {0,0,0};

	uchar* imgcopy = new uchar[height*step];
	memcpy(imgcopy, img->imageData, height*step);

	mask_t maskData = {mask, maskWidth, maskHeight};

	int x = 100;
	int y = 100;

	applyMotionMask(maskData, masked1, masked2, imgcopy, data, step, height, x, y, x+5*3, y+5);
	applyMotionMask(maskData, masked1, masked2, imgcopy, data, step, height, x+5*3, y+5, x+10*3, y+10);
	applyMotionMask(maskData, masked1, masked2, imgcopy, data, step, height, x+10*3, y+10, x+15*3, y+15);


#if 0
	for(int y=0; y < height; y++) {
		for(int x=0; x < step; x+=3) {
			v = applyMask(mask, masked, imgcopy, x, y, maskWidth, maskHeight, step, height);

			img->imageData[y * step + x+0] = v.b;
			img->imageData[y * step + x+1] = v.g;
			img->imageData[y * step + x+2] = v.r;
		}
	}
#endif


	cvNamedWindow("image display", CV_WINDOW_AUTOSIZE);
	cvMoveWindow("image display", 100, 100);

	cvShowImage("image display", img);

	cvWaitKey(0);

	cvReleaseImage(&img);

	return 0;
}
