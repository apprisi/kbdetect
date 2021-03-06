#include "detector.h"
#include <ctime>
#include <sys/time.h>
#include <iostream>
#include <fstream>

Detector::Detector(){
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	ifstream fin;
	fin.open("detector.cfg");
	if (fin.fail()){
		cout<<"Cannot open detector configuration file"<<endl;
		exit(1);
	}
	
	string cascade;
	string intradetect;
	string intratrack;
	dtype = UNKNOWN;
	//parse parameters
	char line[1024];
	fin.getline(line,1024);
	string dt;
	while(!fin.eof()){
		char *tok;
		tok = strtok(line, " ");
		if (strcmp(tok,"TYPE")==0){
			dt = strtok(NULL, " ");
		}		
		if (strcmp(tok,"CASCADE")==0){
			cascade = strtok(NULL, " ");
		}
		else if (strcmp(tok,"INTRADETECT")==0){
			intradetect = strtok(NULL, " ");
		}
		else if (strcmp(tok,"INTRATRACK")==0){
			intratrack = strtok(NULL, " ");
		}		
		fin.getline(line,1024);
	}
	if (strcmp(dt.data(),"SZU")==0){
		faceCascade = LoadMBLBPCascade(cascade.data());
		dtype = DSZU;
	}
	else if (strcmp(dt.data(),"OPENCV")==0){
		HaarCascade = (CvHaarClassifierCascade*)cvLoad(cascade.data(), 0, 0, 0);
		dtype = DOPENCV;
	}
	else{
		cout<<"Unknown detector cascade type"<<dtype<<endl;
		exit(1);
	}
	if(faceCascade == NULL)
    {
        printf("Couldn't load Face detector '%s'\n", cascade.data());
        exit(1);
    }
	
	xxd = new XXDescriptor(4);
	faceLandmark = new FaceAlignment(intradetect.data(), intratrack.data(), xxd);
	if (!faceLandmark->Initialized()) {
		cerr << "FaceAlignment cannot be initialized." << endl;
		exit(1);
	}
	gettimeofday(&end, NULL);
	double elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Detection initialized in "<<elapsed<<" seconds"<<endl;
}

Mat Detector::detect(const string imgname, int numLandmarks){
	Mat resized;
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	
	IplImage *frame = cvLoadImage(imgname.data(), 2|4);
	if (frame == NULL)
    {
      fprintf(stderr, "Cannot open image %s.Returning empty Mat...\n", imgname.data());
      return resized;
    }
	
	else if (frame->width < 50 || frame->height < 50)
    {
      fprintf(stderr, "image %s too small.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }	
	else if (frame->width > 100000 || frame->height > 100000)
    {
      fprintf(stderr, "image %s too large.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }
	
	// convert image to grayscale
    IplImage *frame_bw = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
    cvConvertImage(frame, frame_bw);
	Mat frame_mat(frame, 1);
	// Smallest face size.
    CvSize minFeatureSize = cvSize(100, 100);
    int flags =  CV_HAAR_DO_CANNY_PRUNING;
    // How detailed should the search be.
    float search_scale_factor = 1.1f;
    CvMemStorage* storage;
    CvSeq* rects;
    int nFaces;
	
    storage = cvCreateMemStorage(0);
    cvClearMemStorage(storage);

    // Detect all the faces in the greyscale image.
	if (dtype == DSZU){
		rects = MBLBPDetectMultiScale(frame_bw, faceCascade, storage, 1229, 1, 50, 500);
	}
	else if (dtype == DOPENCV){
		rects = cvHaarDetectObjects(frame_bw, HaarCascade, storage, search_scale_factor, 2, flags, minFeatureSize);
	}
	else{
		cout<<"Unknown detector type: "<<dtype<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    double elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face detected in "<<elapsed<<" seconds"<<endl;	
	gettimeofday(&begin, NULL);
	nFaces = rects->total;

	if (nFaces != 1){
		//storage = cvCreateMemStorage(0);
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&frame_bw);
		cvReleaseImage(&frame);	
		return resized;
	}
		
	int iface = 0;
	CvRect *r = (CvRect*)cvGetSeqElem(rects, iface);
	
	//Face landmark detection
	float score, notFace = 0.5;
	Mat X;
	Rect rect(r->x, r->y, r->width, r->height);
	INTRAFACE::HeadPose hp;

	gettimeofday(&begin, NULL);
	if (faceLandmark->Detect(frame_mat, rect, X, score) == INTRAFACE::IF_OK)
	{
		faceLandmark->EstimateHeadPose(X,hp);
		// only draw valid faces
		if (score >= notFace) {
			for (int i = 0 ; i < X.cols ; i++)
				cv::circle(frame_mat,cv::Point((int)X.at<float>(0,i), (int)X.at<float>(1,i)), 2, cv::Scalar(0,255,0), -1);
		}
		else{
			cout<<"False positive face"<<endl;
			return resized;
		}
	}
	else
	{
		cout<<"Landmark detection failed"<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Landmarks detected in "<<elapsed<<" seconds"<<endl;
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&frame_bw);
	cvReleaseImage(&frame);
	return frame_mat;
}

Mat Detector::detect(const string imgname, Mat& landmarks, int* pose, int numLandmarks){
	Mat resized;
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	
	IplImage *frame = cvLoadImage(imgname.data(), 2|4);
	if (frame == NULL)
    {
      fprintf(stderr, "Cannot open image %s.Returning empty Mat...\n", imgname.data());
      return resized;
    }
	
	else if (frame->width < 50 || frame->height < 50)
    {
      fprintf(stderr, "image %s too small.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }	
	else if (frame->width > 100000 || frame->height > 100000)
    {
      fprintf(stderr, "image %s too large.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }
	
	// convert image to grayscale
    IplImage *frame_bw = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
    cvConvertImage(frame, frame_bw);
	Mat frame_mat(frame, 1);
	// Smallest face size.
    CvSize minFeatureSize = cvSize(100, 100);
    int flags =  CV_HAAR_DO_CANNY_PRUNING;
    // How detailed should the search be.
    float search_scale_factor = 1.1f;
    CvMemStorage* storage;
    CvSeq* rects;
    int nFaces;
	
    storage = cvCreateMemStorage(0);
    cvClearMemStorage(storage);

    // Detect all the faces in the greyscale image.
	if (dtype == DSZU){
		rects = MBLBPDetectMultiScale(frame_bw, faceCascade, storage, 1229, 1, 50, 500);
	}
	else if (dtype == DOPENCV){
		rects = cvHaarDetectObjects(frame_bw, HaarCascade, storage, search_scale_factor, 2, flags, minFeatureSize);
	}
	else{
		cout<<"Unknown detector type: "<<dtype<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    double elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face detected in "<<elapsed<<" seconds"<<endl;	
	gettimeofday(&begin, NULL);
	nFaces = rects->total;

	if (nFaces != 1){
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&frame_bw);
		cvReleaseImage(&frame);	
		return resized;
	}
		
	int iface = 0;
	CvRect *r = (CvRect*)cvGetSeqElem(rects, iface);
	
	//Face landmark detection
	float score, notFace = 0.5;
	Rect rect(r->x, r->y, r->width, r->height);
	INTRAFACE::HeadPose hp;

	gettimeofday(&begin, NULL);
	if (faceLandmark->Detect(frame_mat, rect, landmarks, score) == INTRAFACE::IF_OK)
	{
		faceLandmark->EstimateHeadPose(landmarks,hp);
		for (int i = 0; i < 3; i++){
			pose[i] = hp.angles[i];
		}
		// only draw valid faces
		if (score >= notFace) {
			for (int i = 0 ; i < landmarks.cols ; i++)
				cv::circle(frame_mat,cv::Point((int)landmarks.at<float>(0,i), (int)landmarks.at<float>(1,i)), 2, cv::Scalar(0,255,0), -1);
		}
		else{
			cout<<"False positive face"<<endl;
			return resized;
		}
	}
	else
	{
		cout<<"Landmark detection failed"<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Landmarks detected in "<<elapsed<<" seconds"<<endl;
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&frame_bw);
	cvReleaseImage(&frame); 

	return frame_mat;
}

bool Detector::detect(const Mat& face, Mat& landmarks, int* pose, int numLandmarks){
	//Face landmark detection
	float score, notFace = 0.5;
	Rect rect(0, 0, face.cols, face.rows);
	INTRAFACE::HeadPose hp;

	if (faceLandmark->Detect(face, rect, landmarks, score) == INTRAFACE::IF_OK)
	{
		faceLandmark->EstimateHeadPose(landmarks,hp);
		for (int i = 0; i < 3; i++){
			pose[i] = hp.angles[i];
		}
		// only draw valid faces
		if (score >= notFace) {
			//for (int i = 0 ; i < landmarks.cols ; i++)
			//	cv::circle(frame_mat,cv::Point((int)landmarks.at<float>(0,i), (int)landmarks.at<float>(1,i)), 2, cv::Scalar(0,255,0), -1);
		}
		else{
			return false;
		}
	}
	else
	{
		return false;
	}

	return true;	
}

Mat Detector::detectNorm(string imgname){
	Mat resized;
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	
	IplImage *frame = cvLoadImage(imgname.data(), 2|4);
	if (frame == NULL)
    {
      fprintf(stderr, "Cannot open image %s.Returning empty Mat...\n", imgname.data());
      return resized;
    }
	
	else if (frame->width < 50 || frame->height < 50)
    {
      fprintf(stderr, "image %s too small.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }	
	else if (frame->width > 100000 || frame->height > 100000)
    {
      fprintf(stderr, "image %s too large.Returning empty Mat...\n", imgname.data());
      cvReleaseImage(&frame);
	  return resized;
    }
	
	// convert image to grayscale
    IplImage *frame_bw = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
    cvConvertImage(frame, frame_bw);
	Mat frame_mat(frame, 1);
	// Smallest face size.
    CvSize minFeatureSize = cvSize(100, 100);
    int flags =  CV_HAAR_DO_CANNY_PRUNING;
    // How detailed should the search be.
    float search_scale_factor = 1.1f;
    CvMemStorage* storage;
    CvSeq* rects;
    int nFaces;
	
    storage = cvCreateMemStorage(0);
    cvClearMemStorage(storage);

    // Detect all the faces in the greyscale image.
	if (dtype == DSZU){
		rects = MBLBPDetectMultiScale(frame_bw, faceCascade, storage, 1229, 1, 50, 500);
	}
	else if (dtype == DOPENCV){
		rects = cvHaarDetectObjects(frame_bw, HaarCascade, storage, search_scale_factor, 2, flags, minFeatureSize);
	}
	else{
		cout<<"Unknown detector type: "<<dtype<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    double elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face detected in "<<elapsed<<" seconds"<<endl;	
	gettimeofday(&begin, NULL);
	nFaces = rects->total;

	if (nFaces != 1){
		//storage = cvCreateMemStorage(0);
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&frame_bw);
		cvReleaseImage(&frame);	
		return resized;
	}
		
	int iface = 0;
	CvRect *r = (CvRect*)cvGetSeqElem(rects, iface);
	
	//Face landmark detection
	float score, notFace = 0.5;
	Mat X;
	Rect rect(r->x, r->y, r->width, r->height);
	INTRAFACE::HeadPose hp;

	gettimeofday(&begin, NULL);
	if (faceLandmark->Detect(frame_mat, rect, X, score) == INTRAFACE::IF_OK)
	{
		faceLandmark->EstimateHeadPose(X,hp);
		// only draw valid faces
		if (score >= notFace) {
			for (int i = 0 ; i < X.cols ; i++)
				cv::circle(frame_mat,cv::Point((int)X.at<float>(0,i), (int)X.at<float>(1,i)), 1, cv::Scalar(0,255,0), -1);
		}
		else{
			cout<<"False positive face"<<endl;
			return resized;
		}
	}
	else
	{
		cout<<"Landmark detection failed"<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Landmarks detected in "<<elapsed<<" seconds"<<endl;
	gettimeofday(&begin, NULL);
	//imwrite( "./tmp/face.jpg" , frame_mat );
	//Face alignment
	int normSize = 100;
	int patchSize = 30;
	//calculate bounding box
	double angle = hp.angles[0];
	Mat rotated = rotateImage(frame_mat, -angle);
	
	//rotate landmarks
	float minx = 10000;
	float maxx = 0;
	float miny = 10000;
	float maxy = 0;
	
	for (unsigned int i = 0; i < X.cols; i++){
		//cout<<X.at<float>(0,i)<<" "<<X.at<float>(1,i)<<endl;
		//rotatePoint(frame_mat, angle, X.at<float>(0,i), X.at<float>(1,i), X.at<float>(0,i), X.at<float>(1,i));
		//cout<<X.at<float>(0,i)<<" "<<X.at<float>(1,i)<<endl;
		//cout<<"bbox: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
		//cout<<landmarks[i]<<" "<<landmarks[i+1]<<endl;
		circle(rotated, Point(X.at<float>(0,i), X.at<float>(1,i)), 2, Scalar(255,0,0));
		if (X.at<float>(0,i)  < minx ){
			minx = X.at<float>(0,i) ;
		}
		if (X.at<float>(0,i) > maxx){
			maxx = X.at<float>(0,i) ;
		}
		
		if (X.at<float>(1,i)  < miny){
			miny = X.at<float>(1,i) ;
		}
		
		if (X.at<float>(1,i) > maxy){
			maxy = X.at<float>(1,i);
		}
	}
	//TODO: validate min max

	//extend to rectangle
	float margin = (maxx - minx) - (maxy - miny);
	if (margin > 0){
		maxy += margin/2;
		miny -= margin/2;
	}
	else{
		maxx -= margin/2;
		minx += margin/2;
	}
	//cout<<"bbox: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
	//extend patch region
	int region = (((double)patchSize)/2 + 1)/(normSize - patchSize - 2)*(maxx - minx);
	cout<<"region: "<<region<<endl;
	minx = minx - region - 1;
	maxx = maxx + region + 1;
	miny = miny - region - 1;
	maxy = maxy + region + 1;
	//cout<<"bounds: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
	if (minx < 0 || miny < 0 || maxx > frame_mat.cols || maxy > frame_mat.rows){
		cout<<"Bounding box out of bound: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
		return resized;
	}
	//resize
	Mat roi(rotated, Rect(minx,miny,maxx-minx,maxy-miny));
	resize(roi, resized, Size(normSize, normSize));
	for (unsigned int i = 0; i < X.cols; i++){
		X.at<float>(0,i) = (X.at<float>(0,i) - minx)/(maxx-minx)*normSize;
		X.at<float>(1,i) = (X.at<float>(1,i)- miny)/(maxy-miny)*normSize;
		circle(resized, Point(X.at<float>(0,i), X.at<float>(1,i)), 2, Scalar(255,0,0));
	}
	//return resized;
	
	//storage = cvCreateMemStorage(0);
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&frame_bw);
	cvReleaseImage(&frame);
	
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face aligned in "<<elapsed<<" seconds"<<endl;	
	return resized;
}



Mat Detector::detectNorm(const string filename, const float faceWidth, const float faceHeight, const float patchSize, Mat& landmarks, int numLandmarks, bool showLandmark){
	Mat resized;
	struct timeval begin, end;
	gettimeofday(&begin, NULL);
	
	IplImage *frame = cvLoadImage(filename.data(), 2|4);
	if (frame == NULL)
    {
      fprintf(stderr, "Cannot open image %s.Returning empty Mat...\n", filename.data());
      return resized;
    }
	
	else if (frame->width < 50 || frame->height < 50)
    {
      fprintf(stderr, "image %s too small.Returning empty Mat...\n", filename.data());
      cvReleaseImage(&frame);
	  return resized;
    }	
	else if (frame->width > 100000 || frame->height > 100000)
    {
      fprintf(stderr, "image %s too large.Returning empty Mat...\n", filename.data());
      cvReleaseImage(&frame);
	  return resized;
    }
	
	// convert image to grayscale
    IplImage *frame_bw = cvCreateImage(cvSize(frame->width, frame->height), IPL_DEPTH_8U, 1);
    cvConvertImage(frame, frame_bw);
	Mat frame_mat(frame, 1);
	// Smallest face size.
    CvSize minFeatureSize = cvSize(100, 100);
    int flags =  CV_HAAR_DO_CANNY_PRUNING;
    // How detailed should the search be.
    float search_scale_factor = 1.1f;
    CvMemStorage* storage;
    CvSeq* rects;
    int nFaces;
	
    storage = cvCreateMemStorage(0);
    cvClearMemStorage(storage);

    // Detect all the faces in the greyscale image.
	if (dtype == DSZU){
		rects = MBLBPDetectMultiScale(frame_bw, faceCascade, storage, 1229, 1, 50, 500);
	}
	else if (dtype == DOPENCV){
		rects = cvHaarDetectObjects(frame_bw, HaarCascade, storage, search_scale_factor, 2, flags, minFeatureSize);
	}
	else{
		cout<<"Unknown detector type: "<<dtype<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    double elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face detected in "<<elapsed<<" seconds"<<endl;
	gettimeofday(&begin, NULL);
	nFaces = rects->total;

	if (nFaces != 1){
		//storage = cvCreateMemStorage(0);
		cvReleaseMemStorage(&storage);
		cvReleaseImage(&frame_bw);
		cvReleaseImage(&frame);	
		return resized;
	}
		
	int iface = 0;
	CvRect *r = (CvRect*)cvGetSeqElem(rects, iface);
	
	//Face landmark detection
	float score, notFace = 0.5;
	Rect rect(r->x, r->y, r->width, r->height);
	INTRAFACE::HeadPose hp;

	gettimeofday(&begin, NULL);
	if (faceLandmark->Detect(frame_mat, rect, landmarks, score) == INTRAFACE::IF_OK)
	{
		faceLandmark->EstimateHeadPose(landmarks,hp);
		// only draw valid faces
		if (score >= notFace) {
			if (showLandmark){
				for (int i = 0 ; i < landmarks.cols ; i++)
					cv::circle(frame_mat,cv::Point((int)landmarks.at<float>(0,i), (int)landmarks.at<float>(1,i)), 2, cv::Scalar(0,255,0), -1);
			}
		}
		else{
			cout<<"False positive face"<<endl;
			return resized;
		}
	}
	else
	{
		cout<<"Landmark detection failed"<<endl;
		return resized;
	}
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Landmarks detected in "<<elapsed<<" seconds"<<endl;
	gettimeofday(&begin, NULL);
	//imwrite( "./tmp/face.jpg" , frame_mat );
	//Face alignment
	int normSize = 100;
	//calculate bounding box
	double angle = hp.angles[0];
	Mat rotated = rotateImage(frame_mat, -angle);
	
	//rotate landmarks
	float minx = 10000;
	float maxx = 0;
	float miny = 10000;
	float maxy = 0;
	
	for (unsigned int i = 0; i < landmarks.cols; i++){
		//cout<<X.at<float>(0,i)<<" "<<X.at<float>(1,i)<<endl;
		rotatePoint(frame_mat, angle, (double)landmarks.at<float>(0,i), (double)landmarks.at<float>(1,i), landmarks.at<float>(0,i), landmarks.at<float>(1,i));
		//cout<<X.at<float>(0,i)<<" "<<X.at<float>(1,i)<<endl;
		//cout<<"bbox: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
		//cout<<landmarks[i]<<" "<<landmarks[i+1]<<endl;
		//circle(rotated, Point(landmarks.at<float>(0,i), landmarks.at<float>(1,i)), 2, Scalar(255,0,0));
		if (landmarks.at<float>(0,i)  < minx ){
			minx = landmarks.at<float>(0,i) ;
		}
		if (landmarks.at<float>(0,i) > maxx){
			maxx = landmarks.at<float>(0,i) ;
		}
		
		if (landmarks.at<float>(1,i)  < miny){
			miny = landmarks.at<float>(1,i) ;
		}
		
		if (landmarks.at<float>(1,i) > maxy){
			maxy = landmarks.at<float>(1,i);
		}
	}

	//TODO: validate min max

	//extend to rectangle
	float whRatio = (faceWidth - patchSize)/(faceHeight - patchSize);
	float curRatio = (maxx-minx)/(maxy-miny);
	if (curRatio >= whRatio){
		//fit width, extend height
		float region =  (maxx-minx)/whRatio -(maxy-miny);
		miny -= region/2;
		maxy += region/2;
	}
	else{
		//fit height, extend width
		float region = (maxy - miny)*whRatio - (maxx-minx);
		minx -= region/2;
		maxx += region/2;
	}
	//extend scaled patch
	float px = (maxx-minx)/(faceWidth-patchSize)*patchSize/2;
	float py = (maxy-miny)/(faceHeight-patchSize)*patchSize/2;
	maxx += px + 2;
	minx -= px + 2;
	maxy += py + 2;
	miny -= py + 2;	
	if (minx < 0 || miny < 0 || maxx > frame_mat.cols || maxy > frame_mat.rows){
		cout<<"Bounding box out of bound: "<<minx<<" "<<miny<<" "<<maxx<<" "<<maxy<<endl;
		return resized;
	}
	
	//resize
	Mat roi(rotated, Rect(minx,miny,maxx-minx,maxy-miny));
	resize(roi, resized, Size(faceWidth, faceHeight));
	cout<<"Num of landmarks: "<<landmarks.cols<<" type: "<<landmarks.type()<<endl;
	for (unsigned int i = 0; i < landmarks.cols; i++){
		landmarks.at<float>(0,i) = (landmarks.at<float>(0,i) - minx)/(maxx-minx)*faceWidth;
		landmarks.at<float>(1,i) = (landmarks.at<float>(1,i)- miny)/(maxy-miny)*faceHeight;
	}
	
	
	if (numLandmarks == 5){
		Mat newLandmarks(2, 5, CV_64F);
		//left eye
		for (int i = 0; i < 2; i++)
			newLandmarks.at<float>(i,0) = (landmarks.at<float>(i,19) + landmarks.at<float>(i,22))/2;
			
		//right eye
		for (int i = 0; i < 2; i++)
			newLandmarks.at<float>(i,1) = (landmarks.at<float>(i,25) + landmarks.at<float>(i,28))/2;		
		
		//nose
		for (int i = 0; i < 2; i++)
			newLandmarks.at<float>(i,2) = landmarks.at<float>(i,13) ;
			
		//mouth left
		for (int i = 0; i < 2; i++)
			newLandmarks.at<float>(i,3) = landmarks.at<float>(i,31) ;		
		
		//mouth right
		for (int i = 0; i < 2; i++)
			newLandmarks.at<float>(i,4) = landmarks.at<float>(i,37) ;
			
		landmarks = newLandmarks;
	}
	if (showLandmark){
		for (unsigned int i = 0; i < landmarks.cols; i++){
			circle(resized, Point(landmarks.at<float>(0,i), landmarks.at<float>(1,i)), 3, Scalar(255,0,0));
		}
	}
	//clear stuff
	//storage = cvCreateMemStorage(0);
	cvReleaseMemStorage(&storage);
	cvReleaseImage(&frame_bw);
	cvReleaseImage(&frame);
	
	gettimeofday(&end, NULL);	
    elapsed = (end.tv_sec - begin.tv_sec) + 
              ((end.tv_usec - begin.tv_usec)/1000000.0);
	cout<<"Face aligned in "<<elapsed<<" seconds"<<endl;	
	return resized;
}

Mat Detector::rotateImage(const Mat& source, double angle)
{
    Point2f src_center(source.cols/2.0F, source.rows/2.0F);
    Mat rot_mat = getRotationMatrix2D(src_center, angle, 1.0);
    Mat dst;
    warpAffine(source, dst, rot_mat, source.size());
    return dst;
}

void Detector::rotatePoint(const Mat& source, double angle, const double& x1, const double& y1, float& x, float& y){
	double x0 = source.cols/2.0F;
	double y0 = source.rows/2.0F;
	x = x1 - x0;
	y = y1 - y0;
	angle = angle * PI / 180;
	double tx = x*cos(angle) - y*sin(angle);
	double ty = x*sin(angle) + y*cos(angle);
	x = tx;
	y = ty;
	x += x0;
	y += y0;
}
