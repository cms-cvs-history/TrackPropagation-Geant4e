//Geant4e
#include "TrackPropagation/Geant4e/interface/Geant4ePropagator.h"
#include "TrackPropagation/Geant4e/interface/ConvertFromToCLHEP.h"
#include "TrackPropagation/Geant4e/interface/Geant4eSteppingAction.h"

//CMSSW
#include "MagneticField/Engine/interface/MagneticField.h"
#include "DataFormats/TrajectorySeed/interface/PropagationDirection.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"
#include "TrackingTools/TrajectoryState/interface/SurfaceSideDefinition.h"
#include "DataFormats/GeometrySurface/interface/Cylinder.h"
#include "DataFormats/GeometrySurface/interface/Plane.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

//Geant4
#include "G4ErrorFreeTrajState.hh"
#include "G4ErrorPlaneSurfaceTarget.hh"
#include "G4ErrorCylSurfaceTarget.hh"

//CLHEP
#include "CLHEP/Units/SystemOfUnits.h"


/** Constructor. 
 */
Geant4ePropagator::Geant4ePropagator(const MagneticField* field,
				     const char* particleName,
				     PropagationDirection dir):
  Propagator(dir),
  theField(field),
  theParticleName(particleName),
  theG4eManager(G4ErrorPropagatorManager::GetErrorPropagatorManager()),
  theSteppingAction(0) {
}

/** Destructor. 
 */
Geant4ePropagator::~Geant4ePropagator() {
  delete theSteppingAction;
}

//
////////////////////////////////////////////////////////////////////////////
//

/** Propagate from a free state (e.g. position and momentum in 
 *  in global cartesian coordinates) to a plane.
 */

TrajectoryStateOnSurface 
Geant4ePropagator::propagate (const FreeTrajectoryState& ftsStart, 
			      const Plane& pDest) const {

  if (!theSteppingAction) {
    theG4eManager->InitGeant4e();
    theSteppingAction = new Geant4eSteppingAction;
    theG4eManager->SetUserAction(theSteppingAction);
  }

  ///////////////////////////////
  // Construct the target surface
  //

  //* Get position and normal (orientation) of the destination plane
  GlobalPoint posPlane = pDest.toGlobal(LocalPoint(0,0,0));
  GlobalVector normalPlane = pDest.toGlobal(LocalVector(0,0,1.)); 
  normalPlane = normalPlane.unit();

  //* Transform this into HepPoint3D and HepNormal3D that define a plane for
  //  Geant4e.
  //  CMS uses cm and GeV while Geant4 uses mm and MeV
  HepPoint3D  surfPos  = 
    TrackPropagation::globalPointToHepPoint3D(posPlane);
  HepNormal3D surfNorm = 
    TrackPropagation::globalVectorToHepNormal3D(normalPlane);

  //DEBUG
  LogDebug("Geant4e") << "G4e -  Destination CMS plane position:" << posPlane << "cm\n"
		      << "G4e -                  (Ro, eta, phi): (" 
		      << posPlane.perp() << " cm, " 
		      << posPlane.eta() << ", " 
		      << posPlane.phi().degrees() << " deg)\n"
		      << "G4e -  Destination G4  plane position: " << surfPos
		      << " mm, Ro = " << surfPos.perp() << " mm";
  LogDebug("Geant4e") << "G4e -  Destination CMS plane normal  : " 
		      << normalPlane << "\n"
		      << "G4e -  Destination G4  plane normal  : " 
		      << normalPlane;
  LogDebug("Geant4e") << "G4e -  Distance from plane position to plane: " 
		      << pDest.localZ(posPlane) << " cm";
  //DEBUG

  //* Set the target surface
  G4ErrorSurfaceTarget* g4eTarget = new G4ErrorPlaneSurfaceTarget(surfNorm,
								  surfPos);
  // theG4eManager->SetTarget(g4eTarget); // Not needed ??
  g4eTarget->Dump("G4e - ");
  //
  ///////////////////////////////

  ///////////////////////////////
  // Find initial point
  //

  // * Get the starting point and direction and convert them to Hep3Vector 
  //   for G4. CMS uses cm and GeV while Geant4 uses mm and MeV
  GlobalPoint  cmsInitPos = ftsStart.position();
  GlobalVector cmsInitMom = ftsStart.momentum();

  Hep3Vector g4InitPos = 
    TrackPropagation::globalPointToHep3Vector(cmsInitPos);
  Hep3Vector g4InitMom = 
    TrackPropagation::globalVectorToHep3Vector(cmsInitMom*GeV);

  //DEBUG
  LogDebug("Geant4e") << "G4e -  Initial CMS point position:" << cmsInitPos 
		      << "cm\n"
		      << "G4e -              (Ro, eta, phi): (" 
		      << cmsInitPos.perp() << " cm, " 
		      << cmsInitPos.eta() << ", " 
		      << cmsInitPos.phi().degrees() << " deg)\n"
		      << "G4e -  Initial G4  point position: " << g4InitPos 
		      << " mm, Ro = " << g4InitPos.perp() << " mm";
  LogDebug("Geant4e") << "G4e -  Initial CMS momentum      :" << cmsInitMom 
		      << "GeV\n"
		      << "G4e -  Initial G4  momentum      : " << g4InitMom 
		      << " MeV";
  LogDebug("Geant4e") << "G4e -  Distance from initial point to plane: " 
		      << pDest.localZ(cmsInitPos) << " cm";
  //DEBUG

  //
  //////////////////////////////

  //////////////////////////////
  // Set particle name
  //
  int charge = ftsStart.charge();
  std::string particleName  = theParticleName;
  if (charge > 0)
    particleName += "+";
  else
    particleName += "-";

  LogDebug("Geant4e") << "G4e -  Particle name: " << particleName;

  //
  ///////////////////////////////

  ///////////////////////////////
  //Set the error and trajectories, and finally propagate
  //
  G4ErrorTrajErr g4error( 5, 1 ); //The error matrix
  LogDebug("Geant4e") << "G4e -  Error matrix: " << g4error;

  G4ErrorFreeTrajState* g4eTrajState = 
    new G4ErrorFreeTrajState(particleName, g4InitPos, g4InitMom, g4error);
  LogDebug("Geant4e") << "G4e -  Traj. State: " << (*g4eTrajState);

  //Set the mode of propagation according to the propagation direction
  G4ErrorMode mode = G4ErrorMode_PropForwards;
  if (propagationDirection() == oppositeToMomentum) {
    mode = G4ErrorMode_PropBackwards;
    LogDebug("Geant4e") << "G4e -  Propagator mode is \'backwards\'";
  }
  else
    LogDebug("Geant4e") << "G4e -  Propagator mode is \'forwards\'";
  //
  //////////////////////////////

  //////////////////////////////
  // Propagate

  int ierr =
    theG4eManager->Propagate( g4eTrajState, g4eTarget, mode);
  LogDebug("Geant4e") << "G4e -  Return error from propagation: " << ierr;
  //
  //////////////////////////////

  //////////////////////////////
  // Retrieve the state in the end from Geant4e, convert them to CMS vectors
  // and points, and build global trajectory parameters.
  // CMS uses cm and GeV while Geant4 uses mm and MeV
  //
  HepPoint3D posEnd = g4eTrajState->GetPosition();
  HepVector3D momEnd = g4eTrajState->GetMomentum();

  GlobalPoint  posEndGV = TrackPropagation::hepPoint3DToGlobalPoint(posEnd);
  GlobalVector momEndGV = TrackPropagation::hep3VectorToGlobalVector(momEnd)/GeV;

  //DEBUG
  LogDebug("Geant4e") << "G4e -  Final CMS point position:" << posEndGV 
		      << "cm\n"
		      << "G4e -            (Ro, eta, phi): (" 
		      << posEndGV.perp() << " cm, " 
		      << posEndGV.eta() << ", " 
		      << posEndGV.phi().degrees() << " deg)\n"
		      << "G4e -  Final G4  point position: " << posEnd 
		      << " mm,\tRo =" << posEnd.perp()  << " mm";
  LogDebug("Geant4e") << "G4e -  Final CMS momentum      :" << momEndGV
		      << "GeV\n"
		      << "G4e -  Final G4  momentum      : " << momEnd 
		      << " MeV";
  LogDebug("Geant4e") << "G4e -  Distance from final point to plane: " 
		      << pDest.localZ(posEndGV) << " cm";
  //DEBUG

  GlobalTrajectoryParameters tParsDest(posEndGV, momEndGV, charge, theField);


  // Get the error covariance matrix from Geant4e. It comes in curvilinear
  // coordinates so use the appropiate CMS class  
  G4ErrorTrajErr g4errorEnd = g4eTrajState->GetError();
  CurvilinearTrajectoryError 
    curvError(TrackPropagation::g4ErrorTrajErrToAlgebraicSymMatrix55(g4errorEnd));


  ////////////////////////////////////////////////////////////////////////
  // WARNING: Since this propagator is not supposed to be used in the   //
  // tracker where special treatment need to be used when arriving to   //
  // a surface, we set the SurfaceSide to atCenterOfSurface.            //
  ////////////////////////////////////////////////////////////////////////
  LogDebug("Geant4e") << "G4e -  SurfaceSide is always atCenterOfSurface after propagation";
  SurfaceSide side = atCenterOfSurface;
  //
  ////////////////////////////////////////////////////////

  return TrajectoryStateOnSurface(tParsDest, curvError, pDest, side);
}


/** Propagate from a free state (e.g. position and momentum in 
 *  in global cartesian coordinates) to a cylinder.
 */
TrajectoryStateOnSurface 
Geant4ePropagator::propagate (const FreeTrajectoryState& ftsStart, 
			      const Cylinder& cDest) const {
  //Get Cylinder parameters.
  //CMS uses cm and GeV while Geant4 uses mm and MeV.
  // - Radius
  G4float radCyl = cDest.radius()*cm;
  // - Position: PositionType & GlobalPoint are Basic3DPoint<float,GlobalTag>
  G4ThreeVector posCyl = 
    TrackPropagation::globalPointToHep3Vector(cDest.position());
  // - Rotation: Type in CMSSW is RotationType == TkRotation<T>, T=float
  G4RotationMatrix rotCyl = 
    TrackPropagation::tkRotationFToHepRotation(cDest.rotation());

  //DEBUG --- Remove at some point
  TkRotation<float>  rotation = cDest.rotation();
  LogDebug("Geant4e") << "G4e -  TkRotation" << rotation;
  LogDebug("Geant4e") << "G4e -  G4Rotation" << rotCyl << "mm";


  //Set the target surface
  G4ErrorSurfaceTarget* g4eTarget = new G4ErrorCylSurfaceTarget(radCyl,	posCyl,
								rotCyl);
  //  theG4eManager->SetTarget(g4eTarget); // Not needed?

  //Get the starting point and direction and convert them to Hep3Vector for G4
  //CMS uses cm and GeV while Geant4 uses mm and MeV
  Hep3Vector g4InitMom = 
    TrackPropagation::globalVectorToHep3Vector(ftsStart.momentum()*GeV);
  Hep3Vector g4InitPos = 
    TrackPropagation::globalPointToHep3Vector(ftsStart.position());

  //Set particle name
  int charge = ftsStart.charge();
  std::string particleName  = theParticleName;
  if (charge > 0)
    particleName += "+";
  else
    particleName += "-";

  //Set the error and trajectories, and finally propagate
  G4ErrorTrajErr g4error( 5, 0 ); //The error matrix
  G4ErrorFreeTrajState* g4eTrajState = 
    new G4ErrorFreeTrajState(particleName, g4InitPos, g4InitMom, g4error);

  //Set the mode of propagation according to the propagation direction
  G4ErrorMode mode = G4ErrorMode_PropForwards;
  if (propagationDirection() == oppositeToMomentum)
    mode = G4ErrorMode_PropBackwards;
    

  //int ierr =
  theG4eManager->Propagate( g4eTrajState, g4eTarget, mode);


  // Retrieve the state in the end from Geant4e, converte them to CMS vectors
  // and points, and build global trajectory parameters
  // CMS uses cm and GeV while Geant4 uses mm and MeV
  HepPoint3D posEnd = g4eTrajState->GetPosition();
  HepVector3D momEnd = g4eTrajState->GetMomentum();

  GlobalPoint  posEndGV = TrackPropagation::hepPoint3DToGlobalPoint(posEnd);
  GlobalVector momEndGV = TrackPropagation::hep3VectorToGlobalVector(momEnd)/GeV;

  GlobalTrajectoryParameters tParsDest(posEndGV, momEndGV, charge, theField);


  // Get the error covariance matrix from Geant4e. It comes in curvilinear
  // coordinates so use the appropiate CMS class  
  G4ErrorTrajErr g4errorEnd = g4eTrajState->GetError();
  CurvilinearTrajectoryError 
    curvError(TrackPropagation::g4ErrorTrajErrToAlgebraicSymMatrix55(g4errorEnd));


  ////////////////////////////////////////////////////////////////////////
  // WARNING: Since this propagator is not supposed to be used in the 
  // tracker where special treatment need to be used when arriving to
  // a surface, we set the SurfaceSide to atCenterOfSurface.
  ////////////////////////////////////////////////////////////////////////
  SurfaceSide side = atCenterOfSurface;

  return TrajectoryStateOnSurface(tParsDest, curvError, cDest, side);
}

//
////////////////////////////////////////////////////////////////////////////
//

/** The methods propagateWithPath() are identical to the corresponding
 *  methods propagate() in what concerns the resulting 
 *  TrajectoryStateOnSurface, but they provide in addition the
 *  exact path length along the trajectory.
 */

std::pair< TrajectoryStateOnSurface, double> 
Geant4ePropagator::propagateWithPath (const FreeTrajectoryState& ftsStart, 
				      const Plane& pDest) const {

  theSteppingAction->reset();

  //Finally build the pair<...> that needs to be returned where the second
  //parameter is the exact path length. Currently calculated with a stepping
  //action that adds up the length of every step
  return TsosPP(propagate(ftsStart,pDest), theSteppingAction->trackLength());
}

std::pair< TrajectoryStateOnSurface, double> 
Geant4ePropagator::propagateWithPath (const FreeTrajectoryState& ftsStart,
				      const Cylinder& cDest) const {
  theSteppingAction->reset();

  //Finally build the pair<...> that needs to be returned where the second
  //parameter is the exact path length. Currently calculated with a stepping
  //action that adds up the length of every step
  return TsosPP(propagate(ftsStart,cDest), theSteppingAction->trackLength());
}
