#pragma once
#include "ksParameterHeaderTask.h"
#include "ksParameterScalarTask.h"
#include "ksParameterSupportTask.h"
#include <array>
#include <cstdint>
namespace ks {
struct ParameterUploadRecipe final {
 enum class Group{Header,Scalar,Pitch,Curve,Modulation,VoiceScale,Gate,Volume,Filter,Support,SignedSquare,Velocity,Time,Lfo,Glide};
 enum class Step{HeaderHeader,ScalarWave128,ScalarEndpoint12,ScalarDoubleEndpoint14,ScalarDoubleEndpoint13,Pitch0,Curve11,Modulation0,ScalarBipolar31,ScalarBipolar32,ScalarBipolar33,VoiceScale14,ScalarWave129,ScalarEndpoint20,ScalarDoubleEndpoint22,ScalarDoubleEndpoint21,Pitch1,Curve19,Modulation1,ScalarBipolar31Lane1,ScalarBipolar32Lane1,ScalarBipolar33Lane1,VoiceScale15,ScalarWave130,ScalarEndpoint28,ScalarDoubleEndpoint30,ScalarDoubleEndpoint29,Pitch2,Curve27,Modulation2,ScalarBipolar31Lane2,ScalarBipolar32Lane2,ScalarBipolar33Lane2,VoiceScale16,ScalarQuarter42,ScalarBipolarHalf146,Gate,ScalarQuarter40,ScalarBipolarHalf144,ScalarQuarter41,ScalarBipolarHalf145,ScalarByteHigh43,Volume44,Filter150,Curve52,Modulation3,ScalarBipolar48,ScalarBipolar49,ScalarBipolar50,SupportRate47,SupportSync140,ScalarSquare56,SignedSquare57,SignedSquare172,Velocity58,Time59,Time60,Time173,Velocity61,Time62,Time63,Time65,ScalarSquare64,SupportClock125,HeaderPart,HeaderByte,Velocity66,Time67,Time68,Time70,SupportRate69,Lfo0,Velocity169,Lfo1,Velocity170,Glide,SupportExit0,Count};
 struct Block{Group group;uint16_t field=0;uint32_t base=0;uint8_t variant=0,multiplier=0,threshold=0;bool optional=false;};
 static constexpr std::array<Block,unsigned(Step::Count)> blocks{{
  {Group::Header,0,0x0,unsigned(ParameterHeaderTask::Kind::Header),0,0,false}, // HeaderHeader
  {Group::Scalar,128,0x0,unsigned(ParameterScalarTask::Kind::Wave),0,0,false}, // ScalarWave128
  {Group::Scalar,12,0x0,unsigned(ParameterScalarTask::Kind::Endpoint),0,0,false}, // ScalarEndpoint12
  {Group::Scalar,14,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint14
  {Group::Scalar,13,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint13
  {Group::Pitch,0,0x0,0,0,0,false}, // Pitch0
  {Group::Curve,11,0x0,0,0,0,false}, // Curve11
  {Group::Modulation,0,0x0,0,0,0,false}, // Modulation0
  {Group::Scalar,31,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar31
  {Group::Scalar,32,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar32
  {Group::Scalar,33,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar33
  {Group::VoiceScale,14,0x0,0,0,0,false}, // VoiceScale14
  {Group::Scalar,129,0x0,unsigned(ParameterScalarTask::Kind::Wave),0,0,false}, // ScalarWave129
  {Group::Scalar,20,0x0,unsigned(ParameterScalarTask::Kind::Endpoint),0,0,false}, // ScalarEndpoint20
  {Group::Scalar,22,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint22
  {Group::Scalar,21,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint21
  {Group::Pitch,0,0x0,1,0,0,false}, // Pitch1
  {Group::Curve,19,0x0,0,0,0,false}, // Curve19
  {Group::Modulation,0,0x0,1,0,0,false}, // Modulation1
  {Group::Scalar,31,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar31Lane1
  {Group::Scalar,32,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar32Lane1
  {Group::Scalar,33,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar33Lane1
  {Group::VoiceScale,15,0x0,0,0,0,false}, // VoiceScale15
  {Group::Scalar,130,0x0,unsigned(ParameterScalarTask::Kind::Wave),0,0,false}, // ScalarWave130
  {Group::Scalar,28,0x0,unsigned(ParameterScalarTask::Kind::Endpoint),0,0,false}, // ScalarEndpoint28
  {Group::Scalar,30,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint30
  {Group::Scalar,29,0x0,unsigned(ParameterScalarTask::Kind::DoubleEndpoint),0,0,false}, // ScalarDoubleEndpoint29
  {Group::Pitch,0,0x0,2,0,0,false}, // Pitch2
  {Group::Curve,27,0x0,0,0,0,false}, // Curve27
  {Group::Modulation,0,0x0,2,0,0,false}, // Modulation2
  {Group::Scalar,31,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar31Lane2
  {Group::Scalar,32,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar32Lane2
  {Group::Scalar,33,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar33Lane2
  {Group::VoiceScale,16,0x0,0,0,0,false}, // VoiceScale16
  {Group::Scalar,42,0x0,unsigned(ParameterScalarTask::Kind::Quarter),0,0,false}, // ScalarQuarter42
  {Group::Scalar,146,0x0,unsigned(ParameterScalarTask::Kind::BipolarHalf),0,0,false}, // ScalarBipolarHalf146
  {Group::Gate,0,0x0,0,0,0,false}, // Gate
  {Group::Scalar,40,0x0,unsigned(ParameterScalarTask::Kind::Quarter),0,0,false}, // ScalarQuarter40
  {Group::Scalar,144,0x0,unsigned(ParameterScalarTask::Kind::BipolarHalf),0,0,false}, // ScalarBipolarHalf144
  {Group::Scalar,41,0x0,unsigned(ParameterScalarTask::Kind::Quarter),0,0,false}, // ScalarQuarter41
  {Group::Scalar,145,0x0,unsigned(ParameterScalarTask::Kind::BipolarHalf),0,0,false}, // ScalarBipolarHalf145
  {Group::Scalar,43,0x0,unsigned(ParameterScalarTask::Kind::ByteHigh),0,0,false}, // ScalarByteHigh43
  {Group::Volume,44,0x0,0,0,0,false}, // Volume44
  {Group::Filter,150,0x0,0,0,0,false}, // Filter150
  {Group::Curve,52,0x0,0,0,0,false}, // Curve52
  {Group::Modulation,0,0x0,3,0,0,false}, // Modulation3
  {Group::Scalar,48,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar48
  {Group::Scalar,49,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar49
  {Group::Scalar,50,0x0,unsigned(ParameterScalarTask::Kind::Bipolar),0,0,false}, // ScalarBipolar50
  {Group::Support,47,0x0,unsigned(ParameterSupportTask::Kind::Rate),0,0,false}, // SupportRate47
  {Group::Support,140,0x0,unsigned(ParameterSupportTask::Kind::Sync),0,0,false}, // SupportSync140
  {Group::Scalar,56,0x0,unsigned(ParameterScalarTask::Kind::Square),0,0,false}, // ScalarSquare56
  {Group::SignedSquare,57,0x0,0,0,0,false}, // SignedSquare57
  {Group::SignedSquare,172,0x0,0,0,0,false}, // SignedSquare172
  {Group::Velocity,58,0x0,0,0,0,false}, // Velocity58
  {Group::Time,59,0x15134,0,107,117,false}, // Time59
  {Group::Time,60,0x15136,0,107,127,false}, // Time60
  {Group::Time,173,0x1513a,0,80,115,true}, // Time173
  {Group::Velocity,61,0x0,0,0,0,false}, // Velocity61
  {Group::Time,62,0x15134,0,107,117,false}, // Time62
  {Group::Time,63,0x15136,0,107,127,false}, // Time63
  {Group::Time,65,0x15136,0,107,127,false}, // Time65
  {Group::Scalar,64,0x0,unsigned(ParameterScalarTask::Kind::Square),0,0,false}, // ScalarSquare64
  {Group::Support,125,0x0,unsigned(ParameterSupportTask::Kind::Clock),0,0,false}, // SupportClock125
  {Group::Header,0,0x0,unsigned(ParameterHeaderTask::Kind::Part),0,0,false}, // HeaderPart
  {Group::Header,0,0x0,unsigned(ParameterHeaderTask::Kind::Byte),0,0,false}, // HeaderByte
  {Group::Velocity,66,0x0,0,0,0,false}, // Velocity66
  {Group::Time,67,0x15134,0,107,117,false}, // Time67
  {Group::Time,68,0x15136,0,107,127,false}, // Time68
  {Group::Time,70,0x15136,0,107,127,false}, // Time70
  {Group::Support,69,0x0,unsigned(ParameterSupportTask::Kind::Rate),0,0,false}, // SupportRate69
  {Group::Lfo,0,0x0,0,0,0,false}, // Lfo0
  {Group::Velocity,169,0x0,0,0,0,false}, // Velocity169
  {Group::Lfo,0,0x0,1,0,0,false}, // Lfo1
  {Group::Velocity,170,0x0,0,0,0,false}, // Velocity170
  {Group::Glide,0,0x0,0,0,0,false}, // Glide
  {Group::Support,0,0x0,unsigned(ParameterSupportTask::Kind::Exit),0,0,false}, // SupportExit0
 }};
};
}
