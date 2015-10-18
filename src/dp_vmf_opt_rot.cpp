/* Copyright (c) 2015, Julian Straub <jstraub@csail.mit.edu> Licensed
 * under the MIT license. See the license file LICENSE.
 */

#include <iostream>
#include <string>
#include "rtDDPvMF/rtDDPvMF.hpp"
//#include "rtDDPvMF/realtimeDDPvMF_openni.hpp"
#include "dpvMFoptRot/dp_vmf_opt_rot.h"

#include <boost/program_options.hpp>
namespace po = boost::program_options;

int main(int argc, char** argv) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help,h", "produce help message")
    ("lambdaDeg,l", po::value<double>(), "lambda in degree for dp and ddp")
    ("in,i", po::value<string>(), "path to input file")
    ("out,o", po::value<string>(), "path to output file")
    ("display,d", "display results")
    ("B,B", po::value<int>(), "B for guided filter")
    ("eps", po::value<float>(), "eps for guided filter")
    ("f_d,f", po::value<float>(), "focal length of depth camera")
    ;

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    std::cout << desc << "\n";
    return 1;
  }

  CfgRtDDPvMF cfg;
  cfg.f_d = 540.;
  cfg.beta = 1e5;
  cfg.nFramesSurvive_ = 1; // DPvMFMM
  cfg.pathOut = std::string("../results/");
  double lambdaDeg = 93.;
  int K = -1;
  if(vm.count("lambdaDeg")) lambdaDeg = vm["lambdaDeg"].as<double>();

  cfg.lambdaFromDeg(lambdaDeg);
  cfg.QfromFrames2Survive(cfg.nFramesSurvive_);

  string path = "";
  //  string mode = "";
  cudaPcl::CfgSmoothNormals cfgNormals;
  cfgNormals.f_d = 540.;
  cfgNormals.eps = 0.2*0.2;
  cfgNormals.B = 9;
  cfgNormals.compress = true;
  uint32_t T = 10;
  //  if(vm.count("mode")) mode = vm["mode"].as<string>();
  if(vm.count("in")) path = vm["in"].as<string>();
  if(vm.count("eps")) cfgNormals.eps = vm["eps"].as<float>();
  if(vm.count("f_d")) cfgNormals.f_d = vm["f_d"].as<float>();
  if(vm.count("B")) cfgNormals.B = uint32_t( vm["B"].as<int>());

  findCudaDevice(argc,(const char**)argv);
  shared_ptr<RtDDPvMF> pRtDDPvMF;

  pRtDDPvMF = shared_ptr<RtDDPvMF>(new RtDDPvMF(cfg,cfgNormals));

  std::cout<<"reading depth image from "<<path<<std::endl;
  cv::Mat depth = cv::imread(path, CV_LOAD_IMAGE_ANYDEPTH);
  std::cout<<"type: "<<int(depth.type()) <<" != "<<int(CV_16U) <<std::endl;

  string pathRgb(path);
  pathRgb.replace(path.length()-5,1,"rgb");
  std::cout<<"reading rgb image from "<<pathRgb<<std::endl;
  cv::Mat gray = cv::imread(pathRgb, CV_LOAD_IMAGE_GRAYSCALE);
  cv::Mat rgb = cv::imread(pathRgb);

  if(vm.count("display")) 
  {
    cv::Mat dI(depth.rows,depth.cols,CV_8UC1);
    depth.convertTo(dI,CV_8UC1,255./4000.,-19.);
    cv::imshow("d",dI);
    cv::imshow("rgb",rgb);
    cv::waitKey(0);
  }
  cv::Mat dI;
  cv::Mat nI;
  cv::Mat zI;
  cv::Mat Iout;
  MatrixXf centroids;
  VectorXf concentrations;
  VectorXf proportions;

  std::cout<<"rtDDPvMFmeans lambdaDeg="<<cfg.lambdaDeg_<<" beta="<<cfg.beta
    <<"nFramesSurvive="<<cfg.nFramesSurvive_<<std::endl;
  std::cout<<"output path: "<<cfg.pathOut<<std::endl;
  for(uint32_t i=0; i<T; ++i)
    pRtDDPvMF->compute(reinterpret_cast<uint16_t*>(depth.data),
        depth.cols,depth.rows);
  Iout = pRtDDPvMF->overlaySeg(rgb,false,true);
  //      cv::Mat Iout = pRtDDPvMF->overlaySeg(gray);
  if(vm.count("display")) 
  {
    dI = pRtDDPvMF->smoothDepthImg();
    nI = pRtDDPvMF->normalsImg();
    zI = pRtDDPvMF->labelsImg(true);
  }
  centroids = pRtDDPvMF->centroids();
  const VectorXu z = pRtDDPvMF->labels();
  K = pRtDDPvMF->GetK();
  proportions = pRtDDPvMF->GetCounts();
  proportions /= proportions.sum();
  concentrations = VectorXf::Zero(K);

  cv::Mat normals = pRtDDPvMF->normalsImgRaw();
  depth = pRtDDPvMF->smoothDepth();
  MatrixXf xSum = MatrixXf::Zero(3,K);
  for (uint32_t i=0; i<normals.cols; ++i) 
    for (uint32_t j=0; j<normals.rows; ++j) 
      if(z(normals.cols*j +i) < K) {
        Eigen::Map<Matrix<float,3,1> > q(&(normals.at<cv::Vec3f>(j,i)[0]));
        float d = depth.at<float>(j,i);
        float scale = d/cfg.f_d;
        xSum.col(z(normals.cols*j +i)) += q*scale;
      }
  std::cout<< pRtDDPvMF->GetxSums() << " vs weighted: " << xSum << std::endl;
  for (uint32_t k = 0; k < K; ++k) 
    concentrations(k) = (xSum.col(k)).norm();

  if(vm.count("display")) 
  {
    cv::imshow("dS",dI);
    cv::imshow("normals",nI);
    cv::imshow("zI",zI);
    cv::imshow("out",Iout);
    cv::waitKey(0);
  }

  if(vm.count("out"))
  {

    std::cout<<" writing out put to "<<std::endl
      <<(vm["out"].as<string>()+"_rgbLabels.png")<<std::endl
      <<vm["out"].as<string>()+"_cRmf.csv"<<std::endl;
    cv::imwrite(vm["out"].as<string>()+"_rgbLabels.png",Iout);
    ofstream out((vm["out"].as<string>()+"_cRmf.csv").data(),
        ofstream::out);
    for(uint32_t i=0; i<centroids.rows();++i) 
    {
      for(uint32_t j=0; j<centroids.cols()-1;++j) 
        out << centroids(i,j)<<" ";
      out << centroids(i,centroids.cols()-1)<<std::endl;
    }
    for(uint32_t i=0; i<concentrations.size()-1;++i) 
      out << concentrations(i) << " ";
    out << concentrations(concentrations.size()-1) << std::endl;
    for(uint32_t i=0; i<proportions.size()-1;++i) 
      out << proportions(i) << " ";
    out << proportions(proportions.size()-1) << std::endl;
    out.close();
  }
  std::cout<<cudaDeviceReset()<<std::endl;
  return (0);
}