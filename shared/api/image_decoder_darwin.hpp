// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
#pragma once

#include <CoreFoundation/CoreFoundation.h>
#include <ImageIO/ImageIO.h>


inline OrtxStatus image_decoder(const ortc::Tensor<uint8_t>& input, ortc::Tensor<uint8_t>& output) {
  const auto& dimensions = input.Shape();
  if (dimensions.size() != 1ULL) {
    return {kOrtxErrorInvalidArgument, "[ImageDecoder]: Only raw image formats are supported."};
  }

  // Get data & the length
  const uint8_t* encoded_image_data = input.Data();
  const int64_t encoded_image_data_len = input.NumberOfElement();

  // check whether it's too small for a image
  if (encoded_image_data_len < 8) {
    return {kOrtxErrorInvalidArgument, "[ImageDecoder]: Invalid image data."};
  }

  OrtxStatus status{};

  CFDataRef         imageData = NULL;
  CGImageRef        image = NULL;
  CGImageSourceRef  imageSource;
  CFDictionaryRef   imageSourceOptions = NULL;
  CFStringRef       optionKeys[2];
  CFTypeRef         optionValues[2];

  optionKeys[0] = kCGImageSourceShouldCache;
  optionValues[0] = (CFTypeRef)kCFBooleanFalse;
  // Only Integer image data is currently supported
  optionKeys[1] = kCGImageSourceShouldAllowFloat;
  optionValues[1] = (CFTypeRef)kCFBooleanFalse;

  imageSourceOptions = CFDictionaryCreate(NULL, (const void **) optionKeys, (const void **) optionValues, 2,
                                          &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);

  imageData = CFDataCreate(NULL, encoded_image_data, encoded_image_data_len);
  if (imageData == nullptr) {
    return {kOrtxErrorInternal, "[ImageDecoder]: Failed to create CFData."};
  }
  imageSource = CGImageSourceCreateWithData(imageData, imageSourceOptions);
  CFRelease(imageData);
  CFRelease(imageSourceOptions);

  if (imageSource == nullptr) {
    return {kOrtxErrorInternal, "[ImageDecoder]: Failed to create CGImageSource."};
  }

  image = CGImageSourceCreateImageAtIndex(imageSource, 0, NULL);
  CFRelease(imageSource);
  if (image == nullptr) {
    return {kOrtxErrorInternal, "[ImageDecoder]: Failed to create CGImage."};
  }

  const int64_t width = static_cast<int64_t>(CGImageGetWidth(image));
  const int64_t height = static_cast<int64_t>(CGImageGetHeight(image));
  const int64_t channels = 3;

  std::vector<int64_t> output_dimensions{height, width, channels};
  uint8_t* decoded_image_data = output.Allocate(output_dimensions);

  // CoreGraphics only support 32BPP with alpha. We get this first and then extract the 24BPP data we need.
  const size_t _32bpp_channel = 4;
  const size_t _32bpp_bytesPerRow = width * _32bpp_channel;
  const size_t bitmapByteCount = width * height * _32bpp_channel;
  void* _32bpp_bitmapData = malloc(bitmapByteCount);

  // Ask for the sRGB color space.
  CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
  if (colorSpace == nullptr) {
    return {kOrtxErrorInternal, "[ImageDecoder]: Failed to create CGColorSpace."};
  }

  CGBitmapInfo _32bpp_bitmapInfo = kCGBitmapByteOrder32Big | (CGBitmapInfo)kCGImageAlphaPremultipliedLast;
  CGContextRef context = CGBitmapContextCreate(_32bpp_bitmapData, width, height, 8 /** bitsPerComponent */,
                                               _32bpp_bytesPerRow, colorSpace, _32bpp_bitmapInfo);
  CFRelease(colorSpace);
  if (context == nullptr) {
    return {kOrtxErrorInternal, "[ImageDecoder]: Failed to create CGBitmapContext."};
  }

  CGRect rect = CGRectMake(0, 0, width, height);
  CGContextDrawImage(context, rect, image);
  CFRelease(context);

  uint8_t *ptr = (uint8_t *)_32bpp_bitmapData;
  for (int i = 0; i < width * height; i++) {
    *(decoded_image_data++) = *(ptr)++; // R
    *(decoded_image_data++) = *(ptr)++; // G
    *(decoded_image_data++) = *(ptr)++; // B
    ptr++; // Skip A
  }

  delete _32bpp_bitmapData;

  return status;
}