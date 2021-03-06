#include "VisualOdometry.h"
#include "GlobalVariables.h"

#include <iomanip>


VisualOdometry::VisualOdometry()
{
    //ctor
}

VisualOdometry::~VisualOdometry()
{
    //dtor
}

void VisualOdometry::Initialize()
{
    num_of_frame = 0;
    num_of_landmarks_in_last_frame = 0;
    grid_size = GRID_SIZE;
}

void VisualOdometry::FindNewLandmarks(std::vector <int>& newLandmarksInd)
{
    // Finding keypoints in grids
    std::vector <cv::KeyPoint> keyPointSet_left, keyPointSet_right;
    cv::Mat dscp_left, dscp_right;
    std::vector <Landmark> landmarkCandidateSet;

//    GridDetect(image_l, keyPointSet_left, dscp_left);
//    GridDetect(image_r, keyPointSet_right, dscp_right);
//
//    // Match stereo features and find landmark candidates
//    MatchStereoFeatures(keyPointSet_left, dscp_left, keyPointSet_right, dscp_right, landmarkCandidateSet);

    GridDetectAndMatch(image_l, image_r, keyPointSet_left, keyPointSet_right, dscp_left, dscp_right, landmarkCandidateSet);

#if DEBUG
    std::cout << "Total " << keyPointSet_left.size() << " keypoints in left image" << std::endl;
    std::cout << "Total " << keyPointSet_right.size() << " keypoints in right image" << std::endl;
#endif // DEBUG

#if DEBUG
    std::cout << landmarkCandidateSet.size() << " candidate landmarks" << std::cout;
    VisualizeLandmarks(image_l, image_r, landmarkCandidateSet);
#endif // DEBUG

    for (unsigned int i=0; i<landmarkCandidateSet.size(); i++){
        this->landmarks.push_back(landmarkCandidateSet[i]);
        newLandmarksInd.push_back(landmarks.size()-1);
    }
}

void VisualOdometry::GridDetect(cv::Mat image, std::vector <cv::KeyPoint>& keyPointSet, cv::Mat& dscp)
{
    cv::Size sizeI = image.size();
    cv::Range rangeRow, rangeCol;
    for (int i=0;i<grid_size;i++){
        for (int j=0;j<grid_size;j++){
            rangeCol = cv::Range(j*(sizeI.width/grid_size), std::min((j+1)*(sizeI.width/grid_size)-1,sizeI.width));
            rangeRow = cv::Range(i*(sizeI.height/grid_size),std::min((i+1)*(sizeI.height/grid_size)-1,sizeI.height));
            cv::Mat subI (image, rangeRow, rangeCol);

#if DEBUG
#endif // DEBUG

            std::vector <cv::KeyPoint> subKeyPoints;
            cv::Mat subDscp;
            FindFeatures(subI, subKeyPoints, subDscp);
            std::cout << subKeyPoints.size() << std::endl;
            for (auto it = subKeyPoints.begin(); it != subKeyPoints.end(); it++){
                it->pt.x += j*sizeI.width/grid_size;
                it->pt.y += i*sizeI.height/grid_size;
                keyPointSet.push_back(*it);
            }
            dscp.push_back(subDscp);
        }
    }
}

void VisualOdometry::GridDetectAndMatch(cv::Mat image_l, cv::Mat image_r,
                                        std::vector <cv::KeyPoint>& keyPointSet_l, std::vector <cv::KeyPoint>& keyPointSet_r,
                                        cv::Mat& dscp_l, cv::Mat& dscp_r, std::vector <Landmark>& landmarkCandidateSet)
{
    cv::Size sizeI = image_l.size();
    cv::Range rangeRow, rangeCol;
    for (int i=0;i<grid_size;i++){
        for (int j=0;j<grid_size;j++){
            rangeCol = cv::Range(j*(sizeI.width/grid_size), std::min((j+1)*(sizeI.width/grid_size)-1, sizeI.width));
            rangeRow = cv::Range(i*(sizeI.height/grid_size), std::min((i+1)*(sizeI.height/grid_size)-1, sizeI.height));
            cv::Mat subI_l (image_l, rangeRow, rangeCol);
            cv::Mat subI_r (image_r, rangeRow, rangeCol);

#if DEBUG
#endif // DEBUG

            std::vector <cv::KeyPoint> subKeyPoints_l, subKeyPoints_r;
            cv::Mat subDscp_l, subDscp_r;
            FindFeatures(subI_l, subKeyPoints_l, subDscp_l);
            FindFeatures(subI_r, subKeyPoints_r, subDscp_r);
            //std::cout << subKeyPoints.size() << std::endl;
            for (auto it = subKeyPoints_l.begin(); it != subKeyPoints_l.end(); it++){
                it->pt.x += j*sizeI.width/grid_size;
                it->pt.y += i*sizeI.height/grid_size;
                keyPointSet_l.push_back(*it);
            }
            dscp_l.push_back(subDscp_l);

            for (auto it = subKeyPoints_r.begin(); it != subKeyPoints_r.end(); it++){
                it->pt.x += j*sizeI.width/grid_size;
                it->pt.y += i*sizeI.height/grid_size;
                keyPointSet_r.push_back(*it);
            }
            dscp_r.push_back(subDscp_r);

            std::vector <Landmark> subLandmarkCandidateSet;
            MatchStereoFeatures(subKeyPoints_l, subDscp_l, subKeyPoints_r, subDscp_r, subLandmarkCandidateSet);
            //VisualizeLandmarks(image_l, image_r, subLandmarkCandidateSet);

            for (unsigned int i=0; i<subLandmarkCandidateSet.size(); i++){
                landmarkCandidateSet.push_back(subLandmarkCandidateSet[i]);
            }
        }
    }
#if DEBUG
            std::cout << "Exiting GridDetectAndMatch" << std::endl;
#endif // DEBUG
}

void VisualOdometry::MatchStereoFeatures(std::vector <cv::KeyPoint> keyPointSet_left,
                                         cv::Mat dscp1,
                                         std::vector <cv::KeyPoint> keyPointSet_right,
                                         cv::Mat dscp2,
                                         std::vector <Landmark>& landmarkCandidateSet)
{
    using namespace cv;

    if (keyPointSet_left.size() == 0 || keyPointSet_right.size() == 0)
        return;

    cv::BFMatcher matcher(cv::NORM_HAMMING);
    std::vector< std::vector<cv::DMatch> > nn_matches;
    matcher.knnMatch(dscp1, dscp2, nn_matches, 2);

#if DEBUG
    std::cout << "Found feature match: " << nn_matches.size() << std::endl;
#endif // DEBUG

    std::vector<cv::KeyPoint> matched1, matched2, inliers1, inliers2;
    std::vector <cv::Mat> matched_dscp1, matched_dscp2, inlier_dscp1, inlier_dscp2;
    std::vector<cv::DMatch> good_matches;
    for(size_t i = 0; i < nn_matches.size(); i++) {
        DMatch first = nn_matches[i][0];
        float dist1 = nn_matches[i][0].distance;
        float dist2 = nn_matches[i][1].distance;

        if(dist1 < nn_match_ratio * dist2) {
            matched1.push_back(keyPointSet_left[first.queryIdx]);
            matched2.push_back(keyPointSet_right[first.trainIdx]);
            matched_dscp1.push_back(dscp1.row(first.queryIdx));
            matched_dscp2.push_back(dscp2.row(first.trainIdx));
        }
    }
    #if DEBUG
            std::cout << "Exiting GridDetectAndMatch" << std::endl;
#endif // DEBUG

    for(unsigned i = 0; i < matched1.size(); i++) {
        double dist = sqrt( pow(matched1[i].pt.x - matched2[i].pt.x, 2) +
                            pow(matched1[i].pt.y - matched2[i].pt.y, 2));

//        std::cout << dist << std::endl;

        if(dist < inlier_threshold) {
            int new_i = static_cast<int>(inliers1.size());
            inliers1.push_back(matched1[i]);
            inliers2.push_back(matched2[i]);
            inlier_dscp1.push_back(matched_dscp1[i]);
            inlier_dscp2.push_back(matched_dscp2[i]);
            good_matches.push_back(DMatch(new_i, new_i, 0));
        }
    }
    #if DEBUG
            std::cout << "Exiting GridDetectAndMatch" << std::endl;
            std::cout << "dscp size: " << dscp1.size().height << std::endl;
            std::cout << "inlier size: " << inliers1.size() << std::endl;
#endif // DEBUG

    for(unsigned int i = 0; i < inliers1.size(); i++) {
        Landmark new_landmark (inliers1[i], inliers2[i], inlier_dscp1[i], inlier_dscp2[i]);
        landmarkCandidateSet.push_back(new_landmark);
    }
#if DEBUG
            std::cout << "Exiting MatchStereoFeature" << std::endl;
#endif // DEBUG
}

void VisualOdometry::FindFeatures(cv::Mat image,
                                 std::vector <cv::KeyPoint>& keypoints,
                                 cv::Mat& dscp)
{
    cv::Ptr<cv::ORB> detector = cv::ORB::create();
//    detector->setPatchSize(5);
    detector->setEdgeThreshold(1);
    detector->detectAndCompute(image,cv::noArray(), keypoints, dscp);
//    for (int i=0;i<keypoints.size();i++){
//        Landmark new_lm (keypoints[i], dscp.row(i));
//        featurePointSet.push_back(new_lm);
//    }
//    return featurePointSet;

#if DEBUG
    //std::cout << "Found " << keypoints.size() << " keypoints in grid." << std::endl;
    //VisualizeLandmarks(image, image, keypoints, keypoints);
#endif // DEBUG
}

void VisualOdometry::ReadNextFrame()
{
    std::ostringstream ss_l, ss_r;
    ss_l << LEFT_PATH << std::setw(4) << std::setfill('0') << num_of_frame + STARTING_FRAME << ".jpg";
    ss_r << RIGHT_PATH << std::setw(4) << std::setfill('0') << num_of_frame + STARTING_FRAME << ".jpg";

    std::string filename_l = ss_l.str();
    std::string filename_r = ss_r.str();

    image_l_last = image_l;
    image_r_last = image_r;

#if DEBUG
    std::cout << "Reading " << ss_l.str() << std::endl;
#endif // DEBUG
    image_l = cv::imread(filename_l, 0);// read the file

#if DEBUG
    std::cout << "Reading " << ss_l.str() << std::endl;
#endif // DEBUG
    image_r = cv::imread(filename_r, 0);// read the file

    if (image_l.empty() || image_r.empty())
        std::cerr << "Error loading stereo image: " << num_of_frame << std::endl;
}

void VisualOdometry::TrackFeatures(std::vector <Landmark>& active_landmarks, cv::Mat i_l, cv::Mat i_l_last,
                                   cv::Mat i_r, cv::Mat i_r_last, std::vector <int>& remain_landmarks)
{
    using namespace cv;
    if (active_landmarks.size() == 0)
        return;

    std::vector <cv::Point2f> kp_l, kp_r, kp_l_last, kp_r_last;
    for (unsigned int i=0;i<active_landmarks.size();i++){
        kp_l_last.push_back(active_landmarks[i].keypoint_l.pt);
        kp_r_last.push_back(active_landmarks[i].keypoint_r.pt);
//        kp_l.push_back(active_landmarks[i].keypoint_l.pt);
//        kp_r.push_back(active_landmarks[i].keypoint_r.pt);
    }
    Mat status_l, err_l, status_r, err_r;

    calcOpticalFlowPyrLK(i_l_last, i_l, kp_l_last, kp_l, status_l, err_l, Size(21, 21));
    calcOpticalFlowPyrLK(i_r_last, i_r, kp_r_last, kp_r, status_r, err_r, Size(21, 21));

#if DEBUG
    std::cout << err_l.size() << std::endl;
    std::cout << err_r.size() << std::endl;
    std::cout << active_landmarks.size() << std::endl;
    std::cout << kp_l_last[0] << " " << kp_l[0] << std::endl;
#endif // DEBUG

    for (unsigned int i=0; i<active_landmarks.size(); i++){
        if (err_l.at<double>(i,1) < TRACKER_ERR_THRESHOLD && err_r.at<double>(i,1) < TRACKER_ERR_THRESHOLD){
            if (kp_l[i].x < 0 || kp_l[i].x > i_l.cols || kp_l[i].y < 0 || kp_l[i].y > i_l.rows ||
                kp_r[i].x < 0 || kp_r[i].x > i_r.cols || kp_r[i].y < 0 || kp_r[i].y > i_r.rows) {
                std::cout << "Throw away landmark " << kp_l[i] << "  " << kp_r[i] << std::endl;
            }
            else {
                landmarks[active_landmarks[i].id].UpdateKeypoint(kp_l[i], kp_r[i], num_of_frame);
                remain_landmarks.push_back(i);
            }
        }
    }
}

void VisualOdometry::InitializeMotionEstimation()
{
    // Finding the Essential matrix
    std::vector <cv::Point2f> points_l, points_r;
    for (unsigned int i=0;i<landmarks.size();i++){
        points_l.push_back(landmarks[i].keypoint_l.pt);
        points_r.push_back(landmarks[i].keypoint_r.pt);
    }

    camera_essential_matrix = cv::findEssentialMat(points_l, points_r, camera_model.focalLength, camera_model.principalPoint);

#if DEBUG
    std::cout << "E: " << camera_essential_matrix << std::endl;
    cv::Mat R, t;
    recoverPose(camera_essential_matrix, points_l, points_r, R, t);
    std::cout << "R: " << R << std::endl;
    std::cout << "t: " << t << std::endl;
#endif

    cv::Mat identity = cv::Mat::eye(3, 3, CV_32F);
    camera_R.push_back(identity);
    camera_t.push_back(cv::Mat::zeros(3, 1, CV_32F));
}

void VisualOdometry::MotionEstimation(std::vector <int> landmarkInd)
/*
    landmarkInd stores the index of shared landmarks between last frame and this frame.
*/
{
    cv::Mat proj;
    std::vector <cv::Point2f> points_current_l, points_last_l, points_last_r;
    for (unsigned int i=0;i<landmarkInd.size();i++){
        int ind = landmarkInd[i];
        points_current_l.push_back(landmarks[ind].keypoint_l.pt);
        points_last_l.push_back(landmarks[ind].traceHistory_l.rbegin()[1].pt);
        points_last_r.push_back(landmarks[ind].traceHistory_r.rbegin()[1].pt);
    }

    cv::Mat camera_matrix_l, camera_matrix_r;
    cv::Mat rt_l, rt_r;
    cv::Mat d = (cv::Mat_<float>(3,1) << -camera_model.baseline, 0, 0);

    cv::hconcat(camera_R[num_of_frame-2], camera_t[num_of_frame-2], rt_l);
    cv::hconcat(camera_R[num_of_frame-2], camera_t[num_of_frame-2] + camera_R[num_of_frame-2] * d, rt_r);

    camera_matrix_l = camera_model.intrinsic*rt_l;
    camera_matrix_r = camera_model.intrinsic*rt_r;

#if DEBUG
    std::cout << "[R t] left = " << rt_l << std::endl;
    std::cout << "[R t] right = " << rt_r << std::endl;
#endif // DEBUG

    // Triangulate point in the last frame
    cv::Mat points_last_4D;

    // World frame triangulation method
    // cv::triangulatePoints(camera_matrix_l, camera_matrix_r, points_last_l, points_last_r, points_last_4D);

    // Incremental camera frame triangulation method
    cv::hconcat(camera_R[0], camera_t[0], rt_l);
    cv::hconcat(camera_R[0], camera_t[0] + d, rt_r);
    camera_matrix_l = camera_model.intrinsic*rt_l;
    camera_matrix_r = camera_model.intrinsic*rt_r;
    cv::triangulatePoints(camera_matrix_l, camera_matrix_r, points_last_l, points_last_r, points_last_4D);

    cv::Mat R_new_q, R_new_incremental, t_new_incremental, R_new, t_new;
    std::vector <cv::Point3f> points_last_3D;
    for (int i=0;i<points_last_4D.cols;i++) {
        cv::Point3f pt;
        pt.x =  points_last_4D.at<float>(0,i) / points_last_4D.at<float>(3,i);
        pt.y = points_last_4D.at<float>(1,i) / points_last_4D.at<float>(3,i);
        pt.z = points_last_4D.at<float>(2,i) / points_last_4D.at<float>(3,i);
        points_last_3D.push_back(pt);
    }

    std::vector <double> distCoeffs;
    cv::Mat pnpInliers;
    cv::solvePnPRansac (points_last_3D, points_current_l, camera_model.intrinsic, distCoeffs, R_new_q, t_new_incremental, false, 100, 1.0, 0.98, pnpInliers);
    cv::Rodrigues(R_new_q, R_new_incremental);
    R_new_incremental.convertTo(R_new_incremental, CV_32F);
    t_new_incremental.convertTo(t_new_incremental, CV_32F);
    camera_R_incremental.push_back(R_new_incremental);
    camera_t_incremental.push_back(t_new_incremental);

    R_new = R_new_incremental*camera_R.back() ;
    t_new = camera_t.back() + camera_R.back().inv()*t_new_incremental;
    camera_R.push_back(R_new);
    camera_t.push_back(t_new);

#if DEBUG
    cv::hconcat(R_new, t_new, proj);
    std::cout << "PnP Total num of feature points: " << points_current_l.size() << "  PnP num of Inliers: " << pnpInliers.size() << std::endl;
    std::cout << "New camera R: " << R_new << std::endl;
    std::cout << "New camera t: " << t_new << std::endl;
#endif

    std::cout << "New camera matrix: " << proj << std::endl;
}

void VisualOdometry::Start()
{
    num_of_frame += 1;

#if DEBUG
    std::cout << "Now start tracking frame: " << num_of_frame << std::endl;
#endif // DEBUG

    ReadNextFrame();

    std::vector <Landmark> active_landmarks;
    if (landmarkMap.size()>0){
        std::cout << "Num of item in landmark map " << num_of_frame - 2 << " is " << landmarkMap[num_of_frame-2].size() << std::endl;
        for (unsigned int i=0;i<landmarkMap[num_of_frame-2].size();i++){
            active_landmarks.push_back(landmarks[landmarkMap[num_of_frame-2][i]]);
        }
    }

#if DEBUG
    std::cout << "Last number of active landmarks: " << num_of_landmarks_in_last_frame << std::endl;
    std::cout << "Number of active landmarks: " << active_landmarks.size() << std::endl;
#endif // DEBUG

    std::vector <int> remain_landmarks;
    TrackFeatures( active_landmarks, image_l, image_l_last, image_r, image_r_last, remain_landmarks);

#if DEBUG
    std::cout << "Remaining num of landmarks: " << remain_landmarks.size() << std::endl;
#endif // DEBUG


    std::vector <int> landmarkInd;
    for (unsigned int i=0; i<remain_landmarks.size(); i++){
        int ind = remain_landmarks[i];
        landmarkInd.push_back(active_landmarks[ind].id);
    }

    num_of_landmarks_in_last_frame = remain_landmarks.size();

    // Check if new landmarks needed
    std::vector <int> newLandmarksInd;
    if (num_of_landmarks_in_last_frame < MIN_NUM_LANDMARKS){
        std::cout << "New Key Frame. " << std::endl;
        key_frames.push_back(num_of_frame);

        std::vector <int> landmarksInd_withNewLandmarks (landmarkInd.begin(), landmarkInd.end());
        FindNewLandmarks(newLandmarksInd);
        for (auto it = newLandmarksInd.begin(); it != newLandmarksInd.end(); it++){
            landmarksInd_withNewLandmarks.push_back(*it);
        }
        landmarkMap.push_back(landmarksInd_withNewLandmarks);
    }
    else {
        landmarkMap.push_back(landmarkInd);
    }

    // Triangulation and motion estimation
    if (num_of_frame == 1) {
        InitializeMotionEstimation();
    }
    else {
        MotionEstimation(landmarkInd);
    }

    VisualizeLandmarks(image_l, image_r, active_landmarks);
}

void VisualOdometry::VisualizeLandmarks(cv::Mat image_l, cv::Mat image_r,
                                        std::vector <Landmark> lm)
{
    using namespace cv;
    if (VISUALIZATION) {
        std::vector <cv::KeyPoint> inliers1, inliers2;
    std::vector <cv::DMatch> good_matches;
    for (unsigned int i=0; i<lm.size(); i++){
        int new_i = static_cast<int>(inliers1.size());
        inliers1.push_back(lm[i].keypoint_l);
        inliers2.push_back(lm[i].keypoint_r);
        good_matches.push_back(cv::DMatch(new_i, new_i, 0));
    }

    Mat res;
    drawMatches(image_l, inliers1, image_r, inliers2, good_matches, res);
    namedWindow( "Display window", CV_WINDOW_AUTOSIZE );// create a window for display.
    imshow( "Display window", res );
    waitKey(0);
    }
}

void VisualOdometry::VisualizeLandmarks(cv::Mat image_l, cv::Mat image_r,
                                        std::vector <cv::KeyPoint> inliers1, std::vector <cv::KeyPoint> inliers2)
{
    using namespace cv;
    if (VISUALIZATION) {
        std::vector <cv::DMatch> good_matches;
    for (unsigned int i=0; i<inliers1.size(); i++){
        good_matches.push_back(cv::DMatch(i, i, 0));
    }

    Mat res;
    drawMatches(image_l, inliers1, image_r, inliers2, good_matches, res);
    namedWindow( "Display window", CV_WINDOW_AUTOSIZE );// create a window for display.
    imshow( "Display window", res );
    waitKey(0);
    }
}

void VisualOdometry::PrintTrajectory(char* filename)
{
    std::ofstream myfile;
    myfile.open (filename);

    for (unsigned int i=0; i<camera_t.size(); i++) {
        myfile << camera_t[i].at<float>(0) << " "
               << camera_t[i].at<float>(1) << " "
               << camera_t[i].at<float>(2) << " " << std::endl;
    }

    myfile.close();
}

