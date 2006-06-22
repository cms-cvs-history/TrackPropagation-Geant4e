#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/EDAnalyzer.h"
#include "FWCore/Framework/interface/ESHandle.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h" //For define_fwk_module

#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"

//- Timing
#include "Utilities/Timing/interface/TimingReport.h"

//- Geometry
#include "DetectorDescription/Core/interface/DDCompactView.h"
#include "Geometry/Records/interface/IdealGeometryRecord.h"
#include "SimG4Core/Geometry/interface/DDDWorld.h"
#include "Geometry/DTGeometry/interface/DTGeometry.h"
#include "Geometry/CSCGeometry/interface/CSCGeometry.h"
#include "Geometry/RPCGeometry/interface/RPCGeometry.h"
#include "Geometry/Records/interface/MuonGeometryRecord.h"
#include "DataFormats/MuonDetId/interface/DTWireId.h"
#include "DataFormats/MuonDetId/interface/RPCDetId.h"
#include "DataFormats/MuonDetId/interface/CSCDetId.h"

//- Magnetic field
#include "MagneticField/Engine/interface/MagneticField.h"
#include "MagneticField/Records/interface/IdealMagneticFieldRecord.h"
#include "SimG4Core/MagneticField/interface/Field.h"
#include "SimG4Core/MagneticField/interface/FieldBuilder.h"

//- Propagator
#include "TrackPropagation/Geant4e/interface/Geant4ePropagator.h"
#include "TrackPropagation/Geant4e/interface/ConvertFromToCLHEP.h"
#include "TrackingTools/TrajectoryState/interface/FreeTrajectoryState.h"
#include "TrackingTools/TrajectoryState/interface/TrajectoryStateOnSurface.h"


//- SimHits, Tracks and Vertices
#include "SimDataFormats/TrackingHit/interface/PSimHit.h"
#include "SimDataFormats/TrackingHit/interface/PSimHitContainer.h"
#include "SimDataFormats/Track/interface/EmbdSimTrack.h"
#include "SimDataFormats/Track/interface/EmbdSimTrackContainer.h"
#include "SimDataFormats/Vertex/interface/EmbdSimVertex.h"
#include "SimDataFormats/Vertex/interface/EmbdSimVertexContainer.h"

//- Geant4
#include "G4TransportationManager.hh"

//#include <iostream>

using namespace std;

class Geant4ePropagatorAnalyzer: public edm::EDAnalyzer {

public:
  explicit Geant4ePropagatorAnalyzer(const edm::ParameterSet&);
  virtual ~Geant4ePropagatorAnalyzer() {}

  virtual void analyze(const edm::Event&, const edm::EventSetup&);
  virtual void endJob() {}
  virtual void beginJob(edm::EventSetup const & iSetup);


protected:

  int theRun;
  int theEvent;

  Propagator* thePropagator;
  std::auto_ptr<sim::FieldBuilder> theFieldBuilder;
  edm::ParameterSet theMagneticFieldPSet;

  bool executedBeginJob;

};


Geant4ePropagatorAnalyzer::Geant4ePropagatorAnalyzer(const edm::ParameterSet& p):
  theRun(-1),
  theEvent(-1),
  thePropagator(0),
  executedBeginJob(false) {

  //debug_ = iConfig.getParameter<bool>("debug");
  theMagneticFieldPSet = p.getParameter<edm::ParameterSet>("MagneticField");

}

void Geant4ePropagatorAnalyzer::beginJob(edm::EventSetup const & iSetup) {
  using namespace edm;

  //- DDDWorld: get the DDCV from the ES and use it to build the World
  ESHandle<DDCompactView> pDD;
  iSetup.get<IdealGeometryRecord>().get(pDD);
  new DDDWorld(&(*pDD));
  LogDebug("Geant4e") << "DDDWorld volume created in DDCompactView: " 
		      << &(*pDD);

  // setup the magnetic field
  ESHandle<MagneticField> pMF;
  iSetup.get<IdealMagneticFieldRecord>().get(pMF);
  LogDebug("Geant4e") << "B-field(T) at (0,0,0)(cm): " 
		      << pMF->inTesla(GlobalPoint(0,0,0));
  
  theFieldBuilder = 
    std::auto_ptr<sim::FieldBuilder>(new sim::FieldBuilder(&(*pMF), 
							   theMagneticFieldPSet));
  G4TransportationManager* tM = 
    G4TransportationManager::GetTransportationManager();
  theFieldBuilder->configure("MagneticFieldType",
			     tM->GetFieldManager(),
			     tM->GetPropagatorInField());
  
  //  G4eManager::GetG4eManager();
  //  G4TransportationManager::GetTransportationManager();

  LogDebug("Geant4e") << "Exiting beginJob.";
  executedBeginJob = true;
}

void Geant4ePropagatorAnalyzer::analyze(const edm::Event& iEvent, 
					const edm::EventSetup& iSetup) {

  using namespace edm;

//   if (!executedBeginJob) {
//     LogWarning("Geant4e") << "beginJob was not executed!!!";
//     //cout << "beginJob was not executed!!!";
//     beginJob(iSetup);
//   }
//   else {
//     //cout << "beginJob was already executed!!!";
//     LogDebug("Geant4e") << "beginJob was already executed!!!";
//   }

//   //cout << "beginJob=" << executedBeginJob << ".";

  ///////////////////////////////////////
  //Construct Magnetic Field
  ESHandle<MagneticField> bField;
  iSetup.get<IdealMagneticFieldRecord>().get(bField);


  ///////////////////////////////////////
  //Build geometry

  //- DT...
  ESHandle<DTGeometry> dtGeomESH;
  iSetup.get<MuonGeometryRecord>().get(dtGeomESH);
  LogDebug("Geant4e") << "Got DTGeometry " << std::endl;

  //- CSC...
  ESHandle<CSCGeometry> cscGeomESH;
  iSetup.get<MuonGeometryRecord>().get(cscGeomESH);
  LogDebug("Geant4e") << "Got CSCGeometry " << std::endl;

  //- RPC...
  ESHandle<RPCGeometry> rpcGeomESH;
  iSetup.get<MuonGeometryRecord>().get(rpcGeomESH);
  LogDebug("Geant4e") << "Got RPCGeometry " << std::endl;


  ///////////////////////////////////////
  //Run/Event information
  theRun = (int)iEvent.id().run();
  theEvent = (int)iEvent.id().event();
  LogDebug("Geant4e") << "Begin for run:event ==" << theRun << ":" << theEvent;


  ///////////////////////////////////////
  //Initialise the propagator
  if (! thePropagator) 
    thePropagator = new Geant4ePropagator(&*bField);





  ///////////////////////////////////////
  //Get the sim tracks & vertices 
  Handle<EmbdSimTrackContainer> simTracks;
  iEvent.getByType<EmbdSimTrackContainer>(simTracks);
  if (! simTracks.isValid() ){
    LogWarning("Geant4e") << "No tracks found" << std::endl;
    return;
  }
  LogDebug("Geant4e") << "Got simTracks of size " << simTracks->size();

  Handle<EmbdSimVertexContainer> simVertices;
  iEvent.getByType<EmbdSimVertexContainer>(simVertices);
  if (! simVertices.isValid() ){
    LogWarning("Geant4e") << "No tracks found" << std::endl;
    return;
  }
  LogDebug("Geant4e") << "Got simVertices of size " << simVertices->size();


  ///////////////////////////////////////
  //Get the sim hits for the different muon parts
  Handle<PSimHitContainer> simHitsDT;
  iEvent.getByLabel("SimG4Object", "MuonDTHits", simHitsDT);
  if (! simHitsDT.isValid() ){
    LogWarning("Geant4e") << "No hits found" << std::endl;
    return;
  }
  LogDebug("Geant4e") << "Got MuonDTHits of size " << simHitsDT->size();

  Handle<PSimHitContainer> simHitsCSC;
  iEvent.getByLabel("SimG4Object", "MuonCSCHits", simHitsCSC);
  if (! simHitsCSC.isValid() ){
    LogWarning("Geant4e") << "No hits found" << std::endl;
    return;
  }
  LogDebug("Geant4e") << "Got MuonCSCHits of size " << simHitsCSC->size();


  Handle<PSimHitContainer> simHitsRPC;
  iEvent.getByLabel("SimG4Object", "MuonRPCHits", simHitsRPC);
  if (! simHitsRPC.isValid() ){
    LogWarning("Geant4e") << "No hits found" << std::endl;
    return;
  }
  LogDebug("Geant4e") << "Got MuonRPCHits of size " << simHitsRPC->size();



  ///////////////////////////////////////
  // Iterate over sim tracks to build the FreeTrajectoryState for
  // for the initial position.
  for(EmbdSimTrackContainer::const_iterator simTracksIt = simTracks->begin(); 
      simTracksIt != simTracks->end(); 
      simTracksIt++){

    //- Timing
    TimeMe tProp("Geant4ePropagatorAnalyzer::analyze::propagate");
    
    //- Check if the track corresponds to a muon
    int trkPDG = simTracksIt->type();
    if (abs(trkPDG) != 13 ) {
      LogDebug("Geant4e") << "Track is not a muon: " << trkPDG << std::endl;
      continue;
    }
    
    //- Get momentum, but only use tracks with P > 2 GeV
    GlobalVector p3T = 
      TrackPropagation::hep3VectorToGlobalVector(simTracksIt->momentum().vect());
    if (p3T.mag() < 2.) 
      continue;

    //- Get index of generated particle. Used further down
    uint trkInd = simTracksIt->genpartIndex();

    //- Vertex fixes the starting point
    int vtxInd = simTracksIt->vertIndex();
    GlobalPoint r3T(0.,0.,0.);
    if (vtxInd < 0)
      LogDebug("Geant4e") << "Track with no vertex, defaulting to (0,0,0)" << std::endl;
    else {
      //seems to be stored in mm --> convert to cm
      r3T = TrackPropagation::hep3VectorToGlobalPoint((*simVertices)[vtxInd].position().vect()*0.1);
    }

    //- Charge
    int charge = trkPDG > 0 ? -1 : 1;

    //- Initial covariance matrix is unity 10-6
    CurvilinearTrajectoryError covT;
    covT *= 1E-6;

    //- Build FreeTrajectoryState
    GlobalTrajectoryParameters trackPars(r3T, p3T, charge, &*bField);
    FreeTrajectoryState ftsTrack(trackPars, covT);


    ////////////////////////////////////////////////
    //- Iterate over Hits in DT and check propagation
    for (PSimHitContainer::const_iterator simHitDTIt = simHitsDT->begin(); 
	 simHitDTIt != simHitsDT->end(); 
	 simHitDTIt++){

      //+ Skip if this hit does not belong to the track
      if (simHitDTIt->trackId() != trkInd ) 
	continue;
      //+ Skip if it is not a muon (this is checked before also)
      if (abs(simHitDTIt->particleType()) != 13) 
	continue;

      //+ Build the surface
      DTWireId wId(simHitDTIt->detUnitId());
      const DTLayer* layer = dtGeomESH->layer(wId);
      if (layer == 0){
	LogDebug("Geant4e") << "Failed to get detector unit" << std::endl;
	continue;
      }
      const Surface& surf = layer->surface();

       //+ Discard hits with very low momentum ???
      GlobalVector p3Hit = surf.toGlobal(simHitDTIt->momentumAtEntry());
      if (p3Hit.mag() < 0.5 ) 
	continue;


      //+ Propagate: Need to explicetely
      TrajectoryStateOnSurface tSOSDest = 
	thePropagator->propagate(ftsTrack, surf);

      //+ Get hit position and extrapolation position to compare
      GlobalPoint posHit    = surf.toGlobal(simHitDTIt->localPosition());
      GlobalPoint posExtrap = tSOSDest.freeState()->position();

      LogDebug("Geant4") << "Diference between hit and final position: " 
			 << (posExtrap - posHit).mag() << " cm.";

    } // <-- for over DT sim hits



  } // <-- for over sim tracks
}


//define this as a plug-in
DEFINE_FWK_MODULE(Geant4ePropagatorAnalyzer)
