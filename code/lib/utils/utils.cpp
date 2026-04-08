#include "utils.hpp"

float sampleNormal(float mean, float stddev){
  float u1 = random(1, 10001) / 10001.0f;
  float u2 = random(0, 10000) / 10000.0f;
  
  float radius = sqrtf(-2.0f * logf(u1));
  float angle = 2.0f * PI_F * u2;
  float z0 = radius * cosf(angle);
  
  return mean + z0 * stddev;
}

float sampleUniform(float min_val, float max_val){
    float u = random(0, 10001) / 10000.0f;
    return min_val + (max_val - min_val) * u;
}

float computeZScore(float sample, float baseline, float stddev){
  constexpr float min_stddev = 0.001f;
  float safe_stddev = fmaxf(stddev, min_stddev);
  return (sample - baseline) / safe_stddev;
}

float positivePart(float x){
  return (x > 0.0f) ? x : 0.0f;
}

float directionalTempScore(float z){
  // temperatura alta = rischio
  return positivePart(z);
}

float directionalHumScore(float z){
  // umidità bassa = rischio
  return positivePart(-z);
}

float directionalGasScore(float z){
  // resistenza ai gas bassa = rischio
  return positivePart(-z);
}
