/*
 * Copyright (c) 2013-2015, Georgia Tech Research Corporation
 * All rights reserved.
 *
 * Author(s): Jeongseok Lee <jslee02@gmail.com>
 *
 * Georgia Tech Graphics Lab and Humanoid Robotics Lab
 *
 * Directed by Prof. C. Karen Liu and Prof. Mike Stilman
 * <karenliu@cc.gatech.edu> <mstilman@cc.gatech.edu>
 *
 * This file is provided under the following "BSD-style" License:
 *   Redistribution and use in source and binary forms, with or
 *   without modification, are permitted provided that the following
 *   conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * This code incorporates portions of Open Dynamics Engine
 *     (Copyright (c) 2001-2004, Russell L. Smith. All rights
 *     reserved.) and portions of FCL (Copyright (c) 2011, Willow
 *     Garage, Inc. All rights reserved.), which were released under
 *     the same BSD license as below
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 *   CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *   MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 *   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 *   USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 *   AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *   ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *   POSSIBILITY OF SUCH DAMAGE.
 */

#include "dart/simulation/World.h"

#include <iostream>
#include <string>
#include <vector>

#include "dart/common/Console.h"
#include "dart/integration/SemiImplicitEulerIntegrator.h"
#include "dart/dynamics/Skeleton.h"
#include "dart/constraint/ConstraintSolver.h"

namespace dart {
namespace simulation {

//==============================================================================
World::World()
  : mNameMgrForSkeletons("skeleton"),
    mGravity(0.0, 0.0, -9.81),
    mTimeStep(0.001),
    mTime(0.0),
    mFrame(0),
    mIntegrator(NULL),
    mConstraintSolver(new constraint::ConstraintSolver(mTimeStep)),
    mRecording(new Recording(mSkeletons))
{
  mIndices.push_back(0);
}

//==============================================================================
World::~World()
{
  delete mConstraintSolver;
  delete mRecording;
}

//==============================================================================
void World::setTimeStep(double _timeStep)
{
  assert(_timeStep > 0.0 && "Invalid timestep.");

  mTimeStep = _timeStep;
//  mConstraintHandler->setTimeStep(_timeStep);
  mConstraintSolver->setTimeStep(_timeStep);
  for (std::vector<dynamics::SkeletonPtr>::iterator it = mSkeletons.begin();
       it != mSkeletons.end(); ++it)
  {
    (*it)->setTimeStep(_timeStep);
  }
}

//==============================================================================
double World::getTimeStep() const
{
  return mTimeStep;
}

//==============================================================================
void World::reset()
{
  mTime = 0.0;
  mFrame = 0;
  mRecording->clear();
}

//==============================================================================
void World::step(bool _resetCommand)
{
  // Integrate velocity for unconstrained skeletons
  for (auto& skel : mSkeletons)
  {
    if (!skel->isMobile())
      continue;

    skel->computeForwardDynamicsRecursionPartB();
    skel->integrateVelocities(mTimeStep);
  }

  // Detect activated constraints and compute constraint impulses
  mConstraintSolver->solve();

  // Compute velocity changes given constraint impulses
  for (auto& skel : mSkeletons)
  {
    if (!skel->isMobile())
      continue;

    if (skel->isImpulseApplied())
    {
      skel->computeImpulseForwardDynamics();
      skel->setImpulseApplied(false);
    }

    skel->integratePositions(mTimeStep);

    if (_resetCommand)
    {
      skel->resetForces();
      skel->clearExternalForces();
//    skel->clearConstraintImpulses();
      skel->resetCommands();
    }
  }

  mTime += mTimeStep;
  mFrame++;
}

//==============================================================================
void World::setTime(double _time)
{
  mTime = _time;
}

//==============================================================================
double World::getTime() const
{
  return mTime;
}

//==============================================================================
int World::getSimFrames() const
{
  return mFrame;
}

//==============================================================================
void World::setGravity(const Eigen::Vector3d& _gravity)
{
  mGravity = _gravity;
  for (std::vector<dynamics::SkeletonPtr>::iterator it = mSkeletons.begin();
       it != mSkeletons.end(); ++it)
  {
    (*it)->setGravity(_gravity);
  }
}

//==============================================================================
const Eigen::Vector3d& World::getGravity() const
{
  return mGravity;
}

//==============================================================================
dynamics::SkeletonPtr World::getSkeleton(size_t _index) const
{
  if(_index < mSkeletons.size())
    return mSkeletons[_index];

  return nullptr;
}

//==============================================================================
dynamics::SkeletonPtr World::getSkeleton(const std::string& _name) const
{
  return mNameMgrForSkeletons.getObject(_name);
}

//==============================================================================
size_t World::getNumSkeletons() const
{
  return mSkeletons.size();
}

//==============================================================================
std::string World::addSkeleton(dynamics::SkeletonPtr _skeleton)
{
  assert(_skeleton != nullptr && "Attempted to add NULL skeleton to world.");

  if(nullptr == _skeleton)
  {
    dtwarn << "[World::addSkeleton] Attempting to add a nullptr Skeleton to "
           << "the world!\n";
    return "";
  }

  // If mSkeletons already has _skeleton, then we do nothing.
  if (find(mSkeletons.begin(), mSkeletons.end(), _skeleton) != mSkeletons.end())
  {
    dtwarn << "[World::addSkeleton] Skeleton named [" << _skeleton->getName()
              << "] is already in the world." << std::endl;
    return _skeleton->getName();
  }

  mSkeletons.push_back(_skeleton);
  _skeleton->setName(mNameMgrForSkeletons.issueNewNameAndAdd(
                       _skeleton->getName(), _skeleton));
  _skeleton->init(mTimeStep, mGravity);
  mIndices.push_back(mIndices.back() + _skeleton->getNumDofs());
  mConstraintSolver->addSkeleton(_skeleton);

  // Update recording
  mRecording->updateNumGenCoords(mSkeletons);

  return _skeleton->getName();
}

//==============================================================================
void World::removeSkeleton(dynamics::SkeletonPtr _skeleton)
{
  assert(_skeleton != nullptr && "Attempted to remove nullptr Skeleton from world");

  if(nullptr == _skeleton)
  {
    dtwarn << "[World::removeSkeleton] Attempting to remove a nullptr Skeleton "
           << "from the world!\n";
    return;
  }

  // Find index of _skeleton in mSkeleton.
  size_t i = 0;
  for (; i < mSkeletons.size(); ++i)
  {
    if (mSkeletons[i] == _skeleton)
      break;
  }

  // If i is equal to the number of skeletons, then _skeleton is not in
  // mSkeleton. We do nothing.
  if (i == mSkeletons.size())
  {
    dtwarn << "[World::removeSkeleton] Skeleton [" << _skeleton->getName()
           << "] is not in the world.\n";
    return;
  }

  // Update mIndices.
  for (++i; i < mSkeletons.size() - 1; ++i)
    mIndices[i] = mIndices[i+1] - _skeleton->getNumDofs();
  mIndices.pop_back();

  // Remove _skeleton from constraint handler.
  mConstraintSolver->removeSkeleton(_skeleton);

  // Remove _skeleton in mSkeletons and delete it.
  mSkeletons.erase(remove(mSkeletons.begin(), mSkeletons.end(), _skeleton),
                   mSkeletons.end());
  // TODO(MXG): This approach invalidates the indices of all Skeletons in the
  // vector that came after the one that was deleted. Now if the user attempts
  // to access those Skeletons by their previously assigned indices, it will not
  // work correctly. This isn't really a problem since the user has the freedom
  // to always access Skeletons by name, but it might be worth mentioning in
  // documentation that the user cannot expect the index of a Skeleton to remain
  // valid after a deletion has happened.

  // Update recording
  mRecording->updateNumGenCoords(mSkeletons);

  // Remove from NameManager
  mNameMgrForSkeletons.removeName(_skeleton->getName());
}

//==============================================================================
std::set<dynamics::SkeletonPtr> World::removeAllSkeletons()
{
  std::set<dynamics::SkeletonPtr> ptrs;
  for(std::vector<dynamics::SkeletonPtr>::iterator it=mSkeletons.begin(),
      end=mSkeletons.end(); it != end; ++it)
    ptrs.insert(*it);

  while (getNumSkeletons() > 0)
    removeSkeleton(getSkeleton(0));

  return ptrs;
}

//==============================================================================
int World::getIndex(int _index) const
{
  return mIndices[_index];
}

//==============================================================================
dynamics::SimpleFramePtr World::getFrame(size_t _index) const
{
  if(_index < mFrames.size())
    return mFrames[_index];

  return NULL;
}

//==============================================================================
dynamics::SimpleFramePtr World::getFrame(const std::string& _name) const
{
  return mNameMgrForFrames.getObject(_name);
}

//==============================================================================
size_t World::getNumFrames() const
{
  return mFrames.size();
}

//==============================================================================
std::string World::addFrame(dynamics::SimpleFramePtr _frame)
{
  assert(_frame != nullptr && "Attempted to add nullptr SimpleFrame to world");

  if(nullptr == _frame)
  {
    dtwarn << "[World::addFrame] Attempting to add a nullptr SimpleFrame to the world!\n";
    return "";
  }

  if( find(mFrames.begin(), mFrames.end(), _frame) != mFrames.end() )
  {
    dtwarn << "[World::addFrame] SimpleFrame named [" << _frame->getName()
           << "] is already in the world.\n";
    return _frame->getName();
  }

  mFrames.push_back(_frame);
  _frame->setName(mNameMgrForFrames.issueNewNameAndAdd(
                     _frame->getName(), _frame));

  return _frame->getName();
}

//==============================================================================
void World::removeFrame(dynamics::SimpleFramePtr _entity)
{
  assert(_entity != nullptr && "Attempted to remove nullptr SimpleFrame from world");

  std::vector<dynamics::SimpleFramePtr>::iterator it =
      find(mFrames.begin(), mFrames.end(), _entity);

  if(it == mFrames.end())
  {
    dtwarn << "[World::removeFrame] Frame named [" << _entity->getName()
           << "] is not in the world.\n";
    return;
  }

  mFrames.erase(remove(mFrames.begin(), mFrames.end(), _entity),
                  mFrames.end());
  // TODO(MXG): Same issue as the one above for removeSkeleton()
}

//==============================================================================
std::set<dynamics::SimpleFramePtr> World::removeAllFrames()
{
  std::set<dynamics::SimpleFramePtr> ptrs;
  for(std::vector<dynamics::SimpleFramePtr>::iterator it=mFrames.begin(),
      end=mFrames.end(); it != end; ++it)
    ptrs.insert(*it);

  while(getNumFrames() > 0)
    removeFrame(getFrame(0));

  return ptrs;
}

//==============================================================================
bool World::checkCollision(bool _checkAllCollisions)
{
  return mConstraintSolver->getCollisionDetector()->detectCollision(
        _checkAllCollisions, false);
}

//==============================================================================
constraint::ConstraintSolver* World::getConstraintSolver() const
{
  return mConstraintSolver;
}

//==============================================================================
void World::bake()
{
  collision::CollisionDetector* cd
      = getConstraintSolver()->getCollisionDetector();
  int nContacts = cd->getNumContacts();
  int nSkeletons = getNumSkeletons();
  Eigen::VectorXd state(getIndex(nSkeletons) + 6 * nContacts);
  for (size_t i = 0; i < getNumSkeletons(); i++)
  {
    state.segment(getIndex(i), getSkeleton(i)->getNumDofs())
        = getSkeleton(i)->getPositions();
  }
  for (int i = 0; i < nContacts; i++)
  {
    int begin = getIndex(nSkeletons) + i * 6;
    state.segment(begin, 3)     = cd->getContact(i).point;
    state.segment(begin + 3, 3) = cd->getContact(i).force;
  }
  mRecording->addState(state);
}

//==============================================================================
Recording* World::getRecording()
{
  return mRecording;
}

}  // namespace simulation
}  // namespace dart
