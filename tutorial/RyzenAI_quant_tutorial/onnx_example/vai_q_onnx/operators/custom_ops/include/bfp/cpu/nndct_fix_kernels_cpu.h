//
// Copyright (C) 2023, Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#ifndef _NNDCT_FIX_KERELS_CPU_H_
#define  NNDCT_FIX_KERELS_CPU_H_

template<typename Real>
int _stochastic_round(const Real& val, int seed) {
  int integerPart = (int)val;
  float decimalPart = val - integerPart;
  unsigned long long clock = (unsigned long long)(time(NULL)) + seed;
  unsigned int clock_int = (unsigned int)clock;
  unsigned long long clock_state = (1664525 * clock_int + 1013904223);
  float state = (float)clock_state / 0xFFFFFFFFU;
  int ret = integerPart + (int)(decimalPart+state);
  //printf("-----val: %f, seed: %d, integerPart: %d, clock: %ld, clock_int: %d, clock_state: %ld, state: %f, ret: %d\n", val, seed, integerPart, clock, clock_int, clock_state, state, ret);
  return ret;
}

template<typename Real>
void _vai_round_cpu(const Real& x, int& y, int method, int seed){
  if(2==method){  //half_up
    if(x<0 && (x-floor(x))==0.5) {
      y = ceil(x);
    }else{
      y = round(x);
    }
  }else if(3==method){ //c++ std::round: negative half_down, positive half_up
    y = round(x);
  }else if(4==method){ // floor
    y = floor(x);
  }else if(5==method){ // negative half_up, positive half_even 
    if(x<0 && (x-floor(x))==0.5) {
      y = ceil(x);
    }else if(x - floor(x) == 0.5){
      if( int(floor(x)) % 2 == 0){
        y = floor(x);
      }else{
        y = ceil(x);
      }
    }else{
      y = round(x);
    }
  }else if(6==method){ // towards zero: negative half_up, positive half_down (vs method 3)
    if(x<0 && (x-floor(x))==0.5) {
      y = ceil(x);
    }else if(x>0 && (x-floor(x))==0.5){
      y = floor(x);
    }else{
      y = round(x);
    }
  }else if(7==method){ // up
    y = ceil(x);
  }else if(8==method){ // half_even
    if(x<0 && (x-floor(x))==0.5) {
      if(int(ceil(x)) % 2 == 0){
        y = ceil(x);
      }else{
        y = floor(x);
      }
    }else if(x - floor(x) == 0.5) {
      if( int(floor(x)) % 2 == 0){
        y = floor(x);
      }else{
        y = ceil(x);
      }
    }else{
      y = round(x);
    }
  }else if(9 == method){ //stochastic_round
    unsigned int clock = (unsigned int)time(NULL);
    unsigned int clock_seed = clock + seed;
    int random = rand_r(&clock_seed);
    Real state = (Real)random/(RAND_MAX);
    y = floor(state+x);
    //printf("clock_seed: %d, random: %ld, RAND_MAX: %ld, state: %f, x: %f, y: %d\n", clock_seed, random, RAND_MAX, state, x, y);
    //y = _stochastic_round(x, seed);
  }
}

template<typename Real>
void _fix_neuron_v2_cpu(const Real& src,int& res,
  int val_min, int val_max,Real val_amp,int zero_point,int method, int seed){
  Real res_real_= src*val_amp;
  //method: 
  // 2: half_up 
  // 3: c++ std::round: negative half_down, positive half_up
  // 4: floor
  // 5: negative half_up, positive half_even
  // 6: towards zero: negative half_up, positive half_down (vs method 3)
  // 7: up
  // 8: half_even 
  // 9: stochastic_round
  _vai_round_cpu(res_real_, res, method, seed);

  res = res + zero_point;
  if (res > val_max) {
      res = val_max;
  } else if (res < val_min) {
      res=val_min;
  }
}


template<typename Real>
void _mapping_sigm_cpu(const Real output_amp,
  const int* map_data,Real &src,Real &dst){
  if (src>=8*output_amp){
    dst=32767;
  }else if (src<-8*output_amp){
    dst=0;
  }else {
    if(src >=0){
      int pos=0;
      if (output_amp>=128){
        pos = floor(src/(output_amp/128));
      }else {
        pos = floor(src*(128/output_amp));
      }
      pos%=1024;
      dst = map_data[1024+pos];
    }else{
      int pos=0;
      if (output_amp >= 128){
        pos = floor(fabs(src)/(output_amp/128));
      }else {
        pos = floor(fabs(src)*(128/output_amp));
      }
      pos%=1024;
      if((src == -8*output_amp) && pos == 0){
        dst = 0;
      } else {
        dst = map_data[1024-pos];
      }
    }
  }
}

template<typename Real>
void _mapping_tanh_cpu(const Real output_amp,
  const int* map_data,Real &src,Real &dst){
  if (src>=4*output_amp){
    dst=32767;
  }else if (src<-4*output_amp){
    dst=-32768;
  }else {
    if (src >= 0){
      int pos=0;
      if (output_amp>=256){
        pos = floor(src/(output_amp/256));
      }else {
        pos = floor(src*(256/output_amp));
      }
      pos%=1024;
      dst = map_data[1024 + pos];    
    } else {
      int pos = 0;
      if (output_amp>=256){
        pos = floor(fabs(src)/(output_amp/256));
      }else {
        pos = floor(fabs(src)*(256/output_amp));
      }
      pos%=1024;
      if((src == -4*output_amp) && (pos == 0)){
        dst = map_data[pos];
      } else {
        dst = map_data[1024 - pos];
      }
    }
  }
}

template<typename Real>
void _mappingI_sigm_cpu(const Real output_fp,
  const int* map_data,Real &src,Real &dst){
  if ((src>>output_fp)>=8){
    dst=32767;
  }else if ((src>>output_fp)<-8){
    dst=0;
  }else {
    int pos=0;
    if (output_fp>=7){
      pos = src >> (output_fp - 7);
    }else {
      pos = src << (7 - output_fp);
    }
    pos%=2048;
    if(pos<0){
      dst=map_data[2048+pos];
    }else{
      dst=map_data[pos];
    }
  }
}

template<typename Real>
void _mappingI_tanh_cpu(const Real output_fp,
  const int* map_data,Real &src,Real &dst){
  if ((src>>output_fp)>=4){
    dst=32767;
  }else if ((src>>output_fp)<-4){
    dst=-32768;
  }else {
    int pos=0;
    if (output_fp>=8){
      pos = src>>(output_fp-8);
    }else {
      pos = src<<(8-output_fp);
    }
    pos%=2048;
    if(pos<0){
      dst=map_data[2048+pos];
    }else{
      dst=map_data[pos];
    }
  }
}

template<typename Real>
void _scaleI_cpu(Real &result,int bitwidth,int shift){
  if(shift>0){
    result<<=shift;
  }else{
    result>>=(-shift);
  }
  int max_val=1<<bitwidth;
  if (result > max_val-1) {
    result=result%max_val-max_val;
  } else if (result < -max_val) {
    result=max_val+result%(-max_val);
  }
}

//check if this cpu kenrnels are needed
template<typename Real>
void _dimi_floor_cpu(Real &result,
  const Real val_amp,const int val_min, const int val_max){

  int result_= floor(result/val_amp);
  if (result_ > val_max) {
    result_ = val_max;
  } else if (result_ < val_min) {
    result_ = val_min;
  }
  result=(Real)(result_);
}

template<typename Real>
void _amp_floor_cpu(Real &result,const Real val_amp,const int val_min,const int val_max){
  int result_= floor(result*val_amp);
  if (result_ > val_max) {
    result_ = val_max;
  } else if (result_ < val_min) {
    result_ = val_min;
  }
  result=(Real)(result_);
}

template<typename Real>
void _dimi_cpu(Real &result,const Real val_amp){
  result /= val_amp;
}

template<typename Real>
void _amp_cpu(Real &result,const Real val_amp){
  result *= val_amp;
}

template<typename Real>
void _floor_cpu(Real &result,const int val_min,const int val_max){
  int result_ = floor(result);
  if (result_ > val_max) {
    result_ = val_max;
  } else if (result_ < val_min) {
    result_ = val_min;
  }
  result=(Real)(result_);
}

template<typename Real>
void _dimiI_cpu(int &result,Real diff_amp){
  int tmp_result = diff_amp>=1? int(result/diff_amp): int(result*diff_amp);
  if(diff_amp>1 && result%(int)diff_amp!=0 &&result<0){
    tmp_result-=1;
  }
  result=tmp_result;
}

template<typename Real>
void _dimiI_floor_cpu(Real &result,
  const Real val_amp,const Real val_min, const Real val_max){
  result/=val_amp;
  if (result > val_max) {
    result = val_max;
  } else if (result < val_min) {
    result = val_min;
  }
}

template<typename Real>
void _fix_neuron_v2_cpu_tmp(Real& result,Real val_amp,
  int val_min, int val_max,bool dimi,bool keep_scale,int method){
  if(0==method){
    result=(!dimi)? result*val_amp:result*(1/val_amp);
  }else if(1==method || 3==method){
    int result_;
    if(1==method){
      result_= (!dimi)? floor(result*val_amp):floor(result*(1/val_amp));
    }else{
      result_= (!dimi)? ceil(result*val_amp):ceil(result*(1/val_amp));
    }
    if (result_ > val_max) {
      result_ = val_max;
    } else if (result_ < val_min) {
      result_ = val_min;
    }
    if(keep_scale){
      result= (!dimi)? Real(result_)*(1/val_amp):Real(result_)*val_amp;
    }else{
      result=result_;
    }
  }else if(2==method){
    Real result_= (0==dimi)? result*val_amp:result*(1/val_amp);
    int fixed_result_;
    if(result_ > val_max) {
      result_ = val_max;
    }else if(result_ < val_min) {
      result_= val_min;
    }else if(result_<0 && (result_-floor(result_))==0.5) {
      fixed_result_ = ceil(result_);
    } else {
      fixed_result_ = round(result_);
    }
    if(keep_scale){
      result= (!dimi)? Real(fixed_result_)*(1/val_amp):Real(fixed_result_)*val_amp;
    }else{
      result=fixed_result_;
    }
  }
}


template<typename Dtype>
void cpu_sigmoid_table_lookup(const int N, 
                              const Dtype* input, 
                              const Dtype* table,
                              Dtype* output,
                              int fragpos);  

template<typename Dtype>
void cpu_tanh_table_lookup(const int N, 
                           const Dtype* input, 
                           const Dtype* table,
                           Dtype* output,
                           int fragpos);  

template<typename Dtype>
void cpu_fix_neuron_v1(const int N, 
                       const Dtype* src,
                       const Dtype* fragpos, 
                       Dtype* dst, 
                       int val_min,
                       int val_max, 
                       int keep_scale, 
                       int method);

template<typename Dtype>
void cpu_vai_round(const int N, 
                   const Dtype* src,
                   Dtype* dst, 
                   int method);

template<typename Dtype>
void cpu_fix_neuron_v2(const int N, 
                       const Dtype* src,
                       Dtype* dst, 
                       int val_min,
                       int val_max, 
                       Dtype val_amp, 
                       int zero_point,
                       int keep_scale, 
                       int method);

template<typename Dtype>
void cpu_fix_neuron_v2_2d(const int N_row, 
                          const int N_col, 
                          const Dtype* src, 
                          Dtype* dst, 
                          int val_min,
                          int val_max, 
                          const Dtype* scale, 
                          const int* zero_point,
                          int keep_scale, 
                          int method);

template<typename Dtype>
void cpu_diff_S(const int N, 
                const Dtype* src, 
                Dtype* buffer, 
                Dtype* output, 
                int bitwidth, 
                int range, 
                int method);

template<typename Dtype>
void cpu_layernorm_isqrt(const int N, 
                         const Dtype* src, 
                         Dtype* dst);

template<typename Dtype>
void cpu_aie_isqrt(const int N, 
                   const Dtype* src, 
                   Dtype* dst);


template<typename Dtype>
void cpu_aie_sqrt(const int N, 
                  const Dtype* src, 
                  Dtype* dst);
#endif //_NNDCT_FIX_KERNELS_CPU_H_
