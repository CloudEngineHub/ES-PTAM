/*
* \file process1_live.cpp
* \brief Build and fuse DSIs
* \author (1) Suman Ghosh
* \date 2024-09-29
* \author (2) Valentina Cavinato
* \date 2024-09-29
* \author (3) Guillermo Gallego
* \date 2024-09-29
* Copyright/Rights of Use:
* 2024, Technische Universität Berlin
* Prof. Guillermo Gallego
* Robotic Interactive Perception
* Marchstrasse 23, Sekr. MAR 5-5
* 10587 Berlin, Germany
*/

#include <mapper_emvs_stereo/process1_live.hpp>
#include <mapper_emvs_stereo/utils.hpp>
#include <mapper_emvs_stereo/mapper.hpp>

#include <opencv2/highgui.hpp>
#include <glog/logging.h>
#include <chrono>

//#define TIMING_LOOP


// Alg 1: fuse DSI across cameras
void process_1(
    const std::string world_frame_id,
    const std::string left_cam_frame_id,
    const image_geometry::PinholeCameraModel& cam0,
    const image_geometry::PinholeCameraModel& cam1,
    const image_geometry::PinholeCameraModel& cam2,
    const std::shared_ptr<tf::Transformer> tf_,
    const std::vector<dvs_msgs::Event>& events0,
    const std::vector<dvs_msgs::Event>& events1,
    const std::vector<dvs_msgs::Event>& events2,
    const EMVS::OptionsDepthMap& opts_depth_map,
    const EMVS::ShapeDSI& dsi_shape,
    EMVS::MapperEMVS& mapper_fused,
    EMVS::MapperEMVS& mapper0,
    EMVS::MapperEMVS& mapper1,
    EMVS::MapperEMVS& mapper2,
    const std::string& out_path,
    ros::Time ts,
    int fusion_method,
    geometry_utils::Transformation& T_rv_w
    )
{
#ifdef TIMING_LOOP
  int nloops = 100;
#endif

  // 1. Back-project events into the DSI
  {
    LOG(INFO) << "Setting DSI reference at specific timestamp: " << ts;

    geometry_utils::Transformation T_w_rv, T_w_l, T_w_r;
    mapper0.getPoseAt(tf_, ts, world_frame_id, left_cam_frame_id, T_w_l);
    mapper1.getPoseAt(tf_, ts, world_frame_id, "dvs1", T_w_r);
//    trajectory1.getPoseAt(ros::Time(t_mid), T_w_r);
    //Put camera somewhere along the baseline between 2 cameras
    Eigen::Matrix4d baseline = Eigen::Matrix4d::Identity(4,4);
    baseline(0, 3) = opts_depth_map.rv_pos;
    geometry_utils::Transformation baselineTransform(baseline);
    T_w_rv = T_w_l * baselineTransform;
    T_rv_w = T_w_rv.inverse();

    // Left camera: back-project events into the DSI
    LOG(INFO) << "Computing DSI for first camera";
    std::chrono::high_resolution_clock::time_point t_start_dsi = std::chrono::high_resolution_clock::now();
#ifdef TIMING_LOOP
    for(int i=1; i<=nloops; i++){
#endif
        mapper0.evaluateDSI(events0, tf_, world_frame_id, left_cam_frame_id,  T_rv_w);
#ifdef TIMING_LOOP
      }
#endif
    std::chrono::high_resolution_clock::time_point t_end_dsi = std::chrono::high_resolution_clock::now();
    auto duration_dsi = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_dsi - t_start_dsi ).count();

    LOG(INFO) << "Time to evaluate DSI: " << duration_dsi << " milliseconds";
    LOG(INFO) << "Number of events processed: " << events0.size() << " events";
    LOG(INFO) << "Number of events processed per second: " << static_cast<float>(events0.size()) / (1000.f * static_cast<float>(duration_dsi)) << " Mev/s";
    LOG(INFO) << "Mean square = " << mapper0.dsi_.computeMeanSquare();

    // Right camera: back-project events into the DSI
    LOG(INFO) << "Computing DSI for second camera";
    t_start_dsi = std::chrono::high_resolution_clock::now();
#ifdef TIMING_LOOP
    for (int i=1; i<=nloops; i++){
#endif
        mapper1.evaluateDSI(events1, tf_, world_frame_id, "dvs1", T_rv_w);
#ifdef TIMING_LOOP
      }
#endif
    t_end_dsi = std::chrono::high_resolution_clock::now();
    duration_dsi = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_dsi - t_start_dsi ).count();
    LOG(INFO) << "Time to evaluate DSI: " << duration_dsi << " milliseconds";
    LOG(INFO) << "Number of events processed: " << events1.size() << " events";
    LOG(INFO) << "Number of events processed per second: " << static_cast<float>(events1.size()) / (1000.f * static_cast<float>(duration_dsi)) << " Mev/s";
    LOG(INFO) << "Mean square = " << mapper1.dsi_.computeMeanSquare();


    if (events2.size()>0){

        // 3rd camera: back-project events into the DSI
        LOG(INFO) << "Computing DSI for third camera";
        t_start_dsi = std::chrono::high_resolution_clock::now();
        mapper2.evaluateDSI(events2, tf_, world_frame_id, "dvs2", T_rv_w);
        t_end_dsi = std::chrono::high_resolution_clock::now();
        duration_dsi = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_dsi - t_start_dsi ).count();
        LOG(INFO) << "Time to evaluate DSI: " << duration_dsi << " milliseconds";
        LOG(INFO) << "Number of events processed: " << events2.size() << " events";
        LOG(INFO) << "Number of events processed per second: " << static_cast<float>(events2.size()) / (1000.f * static_cast<float>(duration_dsi)) << " Mev/s";
        LOG(INFO) << "Mean square = " << mapper2.dsi_.computeMeanSquare();
      }
  }

  // set up prefix including output path
//  std::stringstream ss;
//  ss << out_path << std::fixed << std::setprecision(9) << std::setfill('0') << std::setw(13) << t_mid;

  // 2. Fuse the DSIs
  {
    mapper_fused.dsi_.resetGrid();
    mapper_fused.dsi_.addTwoGrids(mapper0.dsi_); // Initialize DSI

    // Sum of the two DSIs
    //mapper_fused.dsi_.addTwoGrids(mapper1.dsi_);

    std::chrono::high_resolution_clock::time_point t_start_fusion = std::chrono::high_resolution_clock::now();
#ifdef TIMING_LOOP
    for (int i=1; i<=nloops; i++){
#endif
        switch(fusion_method){
          case 1:
            mapper_fused.dsi_.minTwoGrids(mapper1.dsi_);
            break;
          case 2:
            mapper_fused.dsi_.harmonicMeanTwoGrids(mapper1.dsi_);
            break;
          case 3:
            mapper_fused.dsi_.geometricMeanTwoGrids(mapper1.dsi_);
            break;
          case 4:
            mapper_fused.dsi_.arithmeticMeanTwoGrids(mapper1.dsi_);
            break;
          case 5:
            mapper_fused.dsi_.rmsTwoGrids(mapper1.dsi_);
            break;
          case 6:
            mapper_fused.dsi_.maxTwoGrids(mapper1.dsi_);
            break;
          default:
            LOG(INFO) << "Improper fusion method selected";
            return;
          }
#ifdef TIMING_LOOP
      }
#endif


    std::chrono::high_resolution_clock::time_point t_end_fusion = std::chrono::high_resolution_clock::now();
    auto t_fusion = std::chrono::duration_cast<std::chrono::milliseconds>(t_end_fusion - t_start_fusion ).count();
    LOG(INFO) << "Time to fuse DSIs: "<<t_fusion <<"ms";

    if (events2.size() > 0){
        LOG(INFO) << "Fusing 3rd DSI";
        switch(fusion_method){
          case 1:
            mapper_fused.dsi_.minTwoGrids(mapper2.dsi_);
            break;
          case 2:
            mapper_fused.dsi_.harmonicMeanTwoGrids(mapper2.dsi_, 3);
            break;
          case 3:
            break;
          case 4:
            break;
          case 5:
            break;
          case 6:
            mapper_fused.dsi_.maxTwoGrids(mapper2.dsi_);
            break;
          default:
            LOG(INFO) << "Improper fusion method selected";
            return;
          }
      }

    // Write the DSI (3D voxel grid) to disk
    //    mapper0.dsi_.writeGridNpy((out_path+std::to_string(t_mid)+"dsi_0.npy").c_str());
    //    mapper1.dsi_.writeGridNpy((out_path+std::to_string(t_mid)+"dsi_1.npy").c_str());
    //    if (events2.size()>0)
    //      mapper2.dsi_.writeGridNpy((out_path+std::to_string(t_mid)+"dsi_2.npy").c_str());
    //    mapper_fused.dsi_.writeGridNpy((out_path+std::to_string(t_mid)+"dsi_fused.npy").c_str());
  }
}
