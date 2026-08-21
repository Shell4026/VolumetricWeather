# Volumetric Weather
### Real-time Atmosphere & Volumetric Cloud Rendering

**Vulkan 1.2 / C++20 / Physically Based Atmosphere / Volumetric Cloud**

https://github.com/user-attachments/assets/3f022400-1da7-4bb3-b4da-c28a405c3c86

## 개요
실시간 게임 환경을 목표로 제작한 Vulkan 기반의 동적 환경 렌더러입니다.

물리 기반 대기와 볼류메트릭 구름을 동일한 태양광, 그림자, 시간 변화, 대기 원근을 공유하는 하나의 환경 시스템으로 통합했습니다.

내부적으로 사용되는 복잡한 물리 파라미터를 직접 노출하는 대신, 아티스트가 결과를 예측하면서 조절할 수 있는 상위 레벨의 파라미터로 변환하여 제공합니다.

### Performance

| Environment       | Result                         |
| ----------------- | ------------------------------ |
| Resolution        | **1920 × 1080 (FHD)**          |
| GPU               | **NVIDIA GeForce RTX 3060 Ti** |
| Weather Rendering | **< 1 ms**                     |

> 성능은 장면 및 렌더링 설정에 따라 달라질 수 있습니다.

## 특징
### Physically Based Atmosphere
Sébastien hillaire의 'A Scalable and Production Ready Sky and Atmosphere Rendering Technique' 기반 실시간 대기 렌더링
- Multiple Scattering LUT
- Transmittance LUT
- Sky-View LUT
- Aerial Perspective LUT
- Volumetric Shadow

### Volumetric Cloud
Ray Marching 실시간 Volumetric 구름 렌더링
- Perlin / Worley Noise 기반
- Temporal Reprojection
  - Neighborhood Clamping
- Depth-Aware Upsampling

### 기타
- Vulkan 1.2
- C++20
- Ray Marching
- Cascaded Shadow Map
- Render Graph
  - Auto Memory Barrier

## 아티스트 파라미터
<img width="300" height="300" alt="image" src="https://github.com/user-attachments/assets/cdcc54bf-63f9-422c-a52f-b2ed4edc9d3c" />
<img width="300" height="300" alt="image" src="https://github.com/user-attachments/assets/08437dd8-4b02-4a59-a6ec-804c0685bffb" />

https://github.com/user-attachments/assets/345f015e-5c10-42fa-8fc5-2cf9fa1c6624

복잡한 내부 파라미터 대신 이를 아티스트 / 사용자가 사용하기 쉽게 래핑하여 제공합니다.

## 빌드
### Requirements
- Windows
- Vulkan SDK
- Visual Studio / MSVC
- CMake
- Ninja

디버그
```console
cmake --preset x64-debug
cmake --build out/build/x64-debug
```
릴리즈
```console
cmake --preset x64-release
cmake --build out/build/x64-release
```
또는 해당 저장소를 다운 받은 후 Visual Studio에서 폴더를 열고 CMake로 빌드

## 외부 라이브러리
- [imgui](https://github.com/ocornut/imgui)
- [nlohmann json](https://github.com/nlohmann/json)
- [stb image](https://github.com/nothings/stb)

## 리소스
- [Mountain - Helindu](https://sketchfab.com/3d-models/mountain-1-3076d5c0f4c9409db806cefd5466cf4a)
- [Postwar City - Aurélien Martel](https://sketchfab.com/3d-models/postwar-city-exterior-scene-30b694d1a4074855a1116a15a0f75731)

## 갤러리
<img width="1906" height="1073" alt="스크린샷 2026-08-19 115001" src="https://github.com/user-attachments/assets/b0a726d4-c273-4ecd-9807-9dffcf6b0c55" />

<img width="1906" height="1073" alt="스크린샷 2026-08-20 183239" src="https://github.com/user-attachments/assets/a58246b3-e16c-4c8b-ba65-666886e63401" />

https://github.com/user-attachments/assets/2cb3ea5e-fcfb-43b8-8553-384fa4c92433
