//===-- VMD/Motion.h - Declares the VMD animation class ------------*- C++ -*-===//
//
//                      The XBeat Project
//
// This file is distributed under the University of Illinois Open Source License.
// See LICENSE.TXT for details.
//
//===-----------------------------------------------------------------------===//
///
/// \file
/// \brief This file declares the VMD::Motion class
///
//===-----------------------------------------------------------------------===//

#pragma once

#include "VMDDefinitions.h"

#include <Node.h>
#include <Resource.h>
#include <Str.h>

namespace VMD {

	/// \brief This is used to perform an animation of a character and/or camera
	///
	/// \remarks A single character may have different motions (for example, one motion is related to walking and another related to wave hands) in effect at the same time.
	class Motion : public Urho3D::Resource
	{
		OBJECT(VMD::Motion);

	public:
		Motion(Urho3D::Context *context);
		virtual ~Motion();
		/// \brief Register object factory.
		static void RegisterObject(Urho3D::Context* context);

		/// \brief Resets the animation state
		void reset();

		/// \brief Loads a motion from a deserializer stream
		///
		/// \param [in] FileName The path of the motion file to be loaded
		/// \returns Whether the loading was successful or not
		virtual bool BeginLoad(Urho3D::Deserializer &source);

		/// \brief Advances the frame of the motion
		///
		/// \param [in] Frames The amount of frames to advance the motion
		/// \returns true if the animation is finished, false otherwise
		bool advanceFrame(float Frames);

		/// \brief Attaches a Camera node to the motion
		///
		/// \param [in] Camera The camera to be attached
		void attachCamera(Urho3D::Node* CameraNode);

		/// \brief Attaches a Model node to the motion
		///
		/// \param [in] Model The model to be attached
		void attachModel(Urho3D::Node* ModelNode);

		/// \brief Returns the motion finished state
		bool isFinished() { return Finished; }

	private:
		/// \brief The current frame of the motion
		float CurrentFrame;
		/// \brief The motion frame count
		float MaxFrame;

		/// \brief Defines whether this motion has finished or not
		bool Finished;

		/// \brief The key frames of bone animations
		Urho3D::HashMap<Urho3D::StringHash, Urho3D::Vector<BoneKeyFrame>> BoneKeyFrames;
		/// \brief The key frames of morphs animations
		Urho3D::HashMap<Urho3D::StringHash, Urho3D::Vector<MorphKeyFrame>> MorphKeyFrames;
		/// \brief The key frames of camera animations
		Urho3D::Vector<CameraKeyFrame> CameraKeyFrames;

		/// \brief The attached cameras
		Urho3D::PODVector<Urho3D::Node*> AttachedCameras;
		/// \brief The attached models
		Urho3D::PODVector<Urho3D::Node*> AttachedModels;

		/// \brief Apply motion parameters to all attached cameras
		void setCameraParameters(float FieldOfView, float Distance, Urho3D::Vector3 &Position, Urho3D::Quaternion &Rotation);

		/// \brief Apply bone deformation parameters to all attached models
		void setBoneParameters(Urho3D::String BoneName, Urho3D::Vector3 &Translation, Urho3D::Quaternion &Rotation);
		
		/// \brief Apply morph parameters to all attached models
		void setMorphParameters(Urho3D::String MorphName, float MorphWeight);

		/// \name Functions extracted from MMDAgent, http://www.mmdagent.jp/
		/// @{

		/// \brief Sets camera parameters according to the motion at the specified frame
		///
		/// \param [in] Frame The frame of the animation
		void updateCamera(float Frame);

		/// \brief Parses the camera interpolation data from the VMD file
		void parseCameraInterpolationData(CameraKeyFrame &Frame, char *InterpolationData);

		/// \brief Sets bones parameters according to the motion at the specified frame
		void updateBones(float Frame);

		/// \brief Parses the bone interpolation data from the VMD file
		void parseBoneInterpolationData(BoneKeyFrame &Frame, char *InterpolationData);

		/// \brief Sets morphs parameters according to the motion at the specified frame
		void updateMorphs(float Frame);

		/// \brief Generates the interpolation data table
		void generateInterpolationTable(Urho3D::PODVector<float> &Table, float X1, float X2, float Y1, float Y2);

		/// \brief Cubic B�zier curve interpolation function
		///
		/// \param [in] T The interpolation value, in [0.0; 1.0] range
		/// \param [in] P1 The first point ordinate
		/// \param [in] P2 The second point ordinate
		float InterpolationFunction(float T, float P1, float P2);

		/// \brief The derivative of the cubic B�zier curve
		/// \sa float VMD::Motion::InterpolationFunction(float T, float P1, float P2)
		float InterpolationFunctionDerivative(float T, float P1, float P2);

		float doLinearInterpolation(float Ratio, float V1, float V2) {
			return V1 * (1.0f - Ratio) + V2 * Ratio;
		}

		/// @}

		enum {
			InterpolationTableSize = 64
		};
	};

}

