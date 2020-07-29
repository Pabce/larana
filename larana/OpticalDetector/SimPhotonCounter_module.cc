// \file SimPhotonCounter.h
// \author Ben Jones, MIT 2010
//
// Module to determine how many phots have been detected at each OpDet
//
// This analyzer takes the SimPhotonsCollection generated by LArG4's sensitive detectors
// and fills up to four trees in the histograms file.  The four trees are:
//
// OpDetEvents       - count how many phots hit the OpDet face / were detected across all OpDet's per event
// OpDets            - count how many phots hit the OpDet face / were detected in each OpDet individually for each event
// AllPhotons      - wavelength information for each phot hitting the OpDet face
// DetectedPhotons - wavelength information for each phot detected
//
// The user may supply a quantum efficiency and sensitive wavelength range for the OpDet's.
// with a QE < 1 and a finite wavelength range, a "detected" phot is one which is
// in the relevant wavelength range and passes the random sampling condition imposed by
// the quantum efficiency of the OpDet
//
// PARAMETERS REQUIRED:
// int32   Verbosity          - whether to write to screen a well as to file. levels 0 to 3 specify different levels of detail to display
// string  InputModule        - the module which produced the SimPhotonsCollection
// bool    MakeAllPhotonsTree - whether to build and store each tree (performance can be enhanced by switching off those not required)
// bool    MakeDetectedPhotonsTree
// bool    MakeOpDetsTree
// bool    MakeOpDetEventsTree
// double  QantumEfficiency   - Quantum efficiency of OpDet
// double  WavelengthCutLow   - Sensitive wavelength range of OpDet
// double  WavelengthCutHigh

// FMWK includes
#include "art/Framework/Core/EDAnalyzer.h"
#include "art/Framework/Core/ModuleMacros.h"
#include "art/Framework/Principal/Event.h"
#include "fhiclcpp/ParameterSet.h"
#include "art/Framework/Principal/Handle.h"
#include "art/Framework/Services/Registry/ServiceHandle.h"
#include "art_root_io/TFileService.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

// LArSoft includes
#include "larana/OpticalDetector/OpDetResponseInterface.h"
#include "larcore/Geometry/Geometry.h"
#include "larcorealg/Geometry/CryostatGeo.h"
#include "larcorealg/Geometry/OpDetGeo.h"
#include "lardataobj/MCBase/MCTrack.h"
#include "lardataobj/Simulation/SimChannel.h"
#include "lardataobj/Simulation/SimPhotons.h"
#include "lardataobj/Simulation/sim.h"
#include "larsim/MCCheater/ParticleInventoryService.h"
#include "larsim/PhotonPropagation/PhotonVisibilityService.h"
#include "larsim/Simulation/LArG4Parameters.h"

#include "nug4/ParticleNavigation/ParticleList.h"
#include "nusimdata/SimulationBase/MCParticle.h"

// ROOT includes
#include "RtypesCore.h"
#include "TH1D.h"
#include "TTree.h"
#include "TLorentzVector.h"
#include "TVector3.h"

// C++ language includes
#include <iostream>
#include <cstring>
#include <cassert>

namespace opdet {

  class SimPhotonCounter : public art::EDAnalyzer{
    public:

      SimPhotonCounter(const fhicl::ParameterSet&);

      void analyze(art::Event const&);

      void beginJob();
      void endJob();

    private:

      /// Threshold used to resolve between visible and ultraviolet light.
      static constexpr double kVisibleThreshold = 200.0; // nm

      /// Value used when a typical visible light wavelength is needed.
      static constexpr double kVisibleWavelength = 450.0; // nm

      /// Value used when a typical ultraviolet light wavelength is needed.
      static constexpr double kVUVWavelength = 128.0; // nm

      // Trees to output

      TTree * fThePhotonTreeAll;
      TTree * fThePhotonTreeDetected;
      TTree * fTheOpDetTree;
      TTree * fTheEventTree;


      // Parameters to read in

      std::vector<std::string> fInputModule;      // Input tag for OpDet collection

      int fVerbosity;                // Level of output to write to std::out

      bool fMakeDetectedPhotonsTree; //
      bool fMakeAllPhotonsTree;      //
      bool fMakeOpDetsTree;         // Switches to turn on or off each output
      bool fMakeOpDetEventsTree;          //

    //  float fQE;                     // Quantum efficiency of tube

    //  float fWavelengthCutLow;       // Sensitive wavelength range
    //  float fWavelengthCutHigh;      //

      TVector3 initialPhotonPosition;
      TVector3 finalPhotonPosition;


      // Data to store in trees

      Float_t fWavelength;
      Float_t fTime;
    //  Int_t fCount;
      Int_t fCountOpDetAll;
      Int_t fCountOpDetDetected;
      Int_t fCountOpDetReflDetected;
      Float_t fT0_vis;

      Int_t fCountEventAll;
      Int_t fCountEventDetected;
    //  Int_t fCountEventDetectedwithRefl;

      Int_t fEventID;
      Int_t fOpChannel;

      //for the analysis tree of the light (gamez)
      bool fMakeLightAnalysisTree;
      std::vector<std::vector<std::vector<double> > > fSignals_vuv;
      std::vector<std::vector<std::vector<double> > > fSignals_vis;

      TTree * fLightAnalysisTree = nullptr;
      int fRun, fTrackID, fpdg, fmotherTrackID;
      double fEnergy, fdEdx;
      std::vector<double> fPosition0;
      std::vector<std::vector<double> > fstepPositions;
      std::vector<double> fstepTimes;
      std::vector<std::vector<double> > fSignalsvuv;
      std::vector<std::vector<double> > fSignalsvis;
      std::string fProcess;

      cheat::ParticleInventoryService const* pi_serv = nullptr;
      phot::PhotonVisibilityService const* fPVS = nullptr;

      bool const fUseLitePhotons;

      void storeVisibility(
        int channel, int nDirectPhotons, int nReflectedPhotons,
        double reflectedT0 = 0.0
        ) const;


      /// Returns if we label as "visibile" a photon with specified wavelength [nm].
      bool isVisible(double wavelength) const
        { return fWavelength < kVisibleThreshold; }


  };
}

namespace opdet {


  SimPhotonCounter::SimPhotonCounter(fhicl::ParameterSet const& pset)
    : EDAnalyzer(pset)
    , fPVS(art::ServiceHandle<phot::PhotonVisibilityService const>().get())
    , fUseLitePhotons(art::ServiceHandle<sim::LArG4Parameters const>()->UseLitePhotons())
  {
    fVerbosity=                pset.get<int>("Verbosity");
    try
    {
       fInputModule = pset.get<std::vector<std::string>>("InputModule",{"largeant"});
    }
    catch(...)
    {
       fInputModule.push_back(pset.get<std::string>("InputModule","largeant"));
    }
    fMakeAllPhotonsTree=       pset.get<bool>("MakeAllPhotonsTree");
    fMakeDetectedPhotonsTree=  pset.get<bool>("MakeDetectedPhotonsTree");
    fMakeOpDetsTree=           pset.get<bool>("MakeOpDetsTree");
    fMakeOpDetEventsTree=      pset.get<bool>("MakeOpDetEventsTree");
    fMakeLightAnalysisTree=    pset.get<bool>("MakeLightAnalysisTree", false);
    //fQE=                       pset.get<double>("QuantumEfficiency");
    //fWavelengthCutLow=         pset.get<double>("WavelengthCutLow");
    //fWavelengthCutHigh=        pset.get<double>("WavelengthCutHigh");


    if (fPVS->IsBuildJob() && fPVS->StoreReflected() && fPVS->StoreReflT0() && fUseLitePhotons) {
      throw art::Exception(art::errors::Configuration)
        << "Building a library with reflected light time is not supported when using SimPhotonsLite.\n";
    }

  }


  void SimPhotonCounter::beginJob()
  {
    // Get file service to store trees
    art::ServiceHandle<art::TFileService const> tfs;
    art::ServiceHandle<geo::Geometry const> geo;

    std::cout<<"Optical Channels positions:  "<<geo->Cryostat(0).NOpDet()<<std::endl;
    for(int ch=0; ch!=int(geo->Cryostat(0).NOpDet()); ch++) {
      double OpDetCenter[3];
      geo->OpDetGeoFromOpDet(ch).GetCenter(OpDetCenter);
      std::cout<<ch<<"  "<<OpDetCenter[0]<<"  "<<OpDetCenter[1]<<"  "<<OpDetCenter[2]<<std::endl;
    }

    double CryoBounds[6];
    geo->CryostatBoundaries(CryoBounds);
    std::cout<<"Cryo Boundaries"<<std::endl;
    std::cout<<"Xmin: "<<CryoBounds[0]<<" Xmax: "<<CryoBounds[1]<<" Ymin: "<<CryoBounds[2]
             <<" Ymax: "<<CryoBounds[3]<<" Zmin: "<<CryoBounds[4]<<" Zmax: "<<CryoBounds[5]<<std::endl;

    try {
      pi_serv = &*(art::ServiceHandle<cheat::ParticleInventoryService const>());
    }
    catch (art::Exception const& e) {
      if (e.categoryCode() != art::errors::ServiceNotFound) throw;
      mf::LogError("SimPhotonCounter")
        << "ParticleInventoryService service is not configured!"
        " Please add it in the job configuration."
        " In the meanwhile, some checks to particles will be skipped."
        ;
    }

    // Create and assign branch addresses to required tree
    if(fMakeAllPhotonsTree)
    {
      fThePhotonTreeAll = tfs->make<TTree>("AllPhotons","AllPhotons");
      fThePhotonTreeAll->Branch("EventID",     &fEventID,          "EventID/I");
      fThePhotonTreeAll->Branch("Wavelength",  &fWavelength,       "Wavelength/F");
      fThePhotonTreeAll->Branch("OpChannel",       &fOpChannel,            "OpChannel/I");
      fThePhotonTreeAll->Branch("Time",        &fTime,             "Time/F");
    }

    if(fMakeDetectedPhotonsTree)
    {
      fThePhotonTreeDetected = tfs->make<TTree>("DetectedPhotons","DetectedPhotons");
      fThePhotonTreeDetected->Branch("EventID",     &fEventID,          "EventID/I");
      fThePhotonTreeDetected->Branch("Wavelength",  &fWavelength,       "Wavelength/F");
      fThePhotonTreeDetected->Branch("OpChannel",       &fOpChannel,            "OpChannel/I");
      fThePhotonTreeDetected->Branch("Time",        &fTime,             "Time/F");
    }

    if(fMakeOpDetsTree)
    {
      fTheOpDetTree    = tfs->make<TTree>("OpDets","OpDets");
      fTheOpDetTree->Branch("EventID",        &fEventID,          "EventID/I");
      fTheOpDetTree->Branch("OpChannel",          &fOpChannel,            "OpChannel/I");
      fTheOpDetTree->Branch("CountAll",       &fCountOpDetAll,      "CountAll/I");
      fTheOpDetTree->Branch("CountDetected",  &fCountOpDetDetected, "CountDetected/I");
      if(fPVS->StoreReflected())
        fTheOpDetTree->Branch("CountReflDetected",  &fCountOpDetReflDetected, "CountReflDetected/I");
      fTheOpDetTree->Branch("Time",                   &fTime,                          "Time/F");
    }

    if(fMakeOpDetEventsTree)
    {
      fTheEventTree  = tfs->make<TTree>("OpDetEvents","OpDetEvents");
      fTheEventTree->Branch("EventID",      &fEventID,            "EventID/I");
      fTheEventTree->Branch("CountAll",     &fCountEventAll,     "CountAll/I");
      fTheEventTree->Branch("CountDetected",&fCountEventDetected,"CountDetected/I");
      if(fPVS->StoreReflected())
        fTheOpDetTree->Branch("CountReflDetected",  &fCountOpDetReflDetected, "CountReflDetected/I");

    }

    //generating the tree for the light analysis:
    if(fMakeLightAnalysisTree)
    {
      fLightAnalysisTree = tfs->make<TTree>("LightAnalysis","LightAnalysis");
      fLightAnalysisTree->Branch("RunNumber",&fRun);
      fLightAnalysisTree->Branch("EventID",&fEventID);
      fLightAnalysisTree->Branch("TrackID",&fTrackID);
      fLightAnalysisTree->Branch("PdgCode",&fpdg);
      fLightAnalysisTree->Branch("MotherTrackID",&fmotherTrackID);
      fLightAnalysisTree->Branch("Energy",&fEnergy);
      fLightAnalysisTree->Branch("dEdx",&fdEdx);
      fLightAnalysisTree->Branch("StepPositions",&fstepPositions);
      fLightAnalysisTree->Branch("StepTimes",&fstepTimes);
      fLightAnalysisTree->Branch("SignalsVUV",&fSignalsvuv);
      fLightAnalysisTree->Branch("SignalsVisible",&fSignalsvis);
      fLightAnalysisTree->Branch("Process",&fProcess);
    }

  }


  void SimPhotonCounter::endJob()
  {
    if(fPVS->IsBuildJob())
    {
      art::ServiceHandle<phot::PhotonVisibilityService>()->StoreLibrary();
    }
  }

  void SimPhotonCounter::analyze(art::Event const& evt)
  {

    // Lookup event ID from event
    art::EventNumber_t event = evt.id().event();
    fEventID=Int_t(event);

    // Service for determining opdet responses
    art::ServiceHandle<opdet::OpDetResponseInterface const> odresponse;

    // get the geometry to be able to figure out signal types and chan -> plane mappings
    art::ServiceHandle<geo::Geometry const> geo;

    // GEANT4 info on the particles (only used if making light analysis tree)
    std::vector<simb::MCParticle> const* mcpartVec = nullptr;

    //-------------------------initializing light tree vectors------------------------
    std::vector<double> totalEnergy_track;
    fstepPositions.clear();
    fstepTimes.clear();
    if (fMakeLightAnalysisTree) {
      mcpartVec = evt.getPointerByLabel<std::vector<simb::MCParticle>>("largeant");

      size_t maxNtracks = 1000U; // mcpartVec->size(); --- { to be fixed soon! ]
      fSignals_vuv.clear();
      fSignals_vuv.resize(maxNtracks);
      fSignals_vis.clear();
      fSignals_vis.resize(maxNtracks);
      for(size_t itrack=0; itrack!=maxNtracks; itrack++) {
        fSignals_vuv[itrack].resize(geo->NOpChannels());
        fSignals_vis[itrack].resize(geo->NOpChannels());
      }
      totalEnergy_track.resize(maxNtracks, 0.);
      //-------------------------stimation of dedx per trackID----------------------
      //get the list of particles from this event
      const sim::ParticleList* plist = pi_serv? &(pi_serv->ParticleList()): nullptr;

      // loop over all sim::SimChannels in the event and make sure there are no
      // sim::IDEs with trackID values that are not in the sim::ParticleList
      std::vector<const sim::SimChannel*> sccol;
      //evt.getView(fG4ModuleLabel, sccol);
     for(auto const& mod : fInputModule){
      evt.getView(mod, sccol);
      double totalCharge=0.0;
      double totalEnergy=0.0;
      //loop over the sim channels collection
      for(size_t sc = 0; sc < sccol.size(); ++sc){
        double numIDEs=0.0;
        double scCharge=0.0;
        double scEnergy=0.0;
        const auto & tdcidemap = sccol[sc]->TDCIDEMap();
        //loop over all of the tdc IDE map objects
        for(auto mapitr = tdcidemap.begin(); mapitr != tdcidemap.end(); mapitr++){
          const std::vector<sim::IDE> idevec = (*mapitr).second;
          numIDEs += idevec.size();
          //go over all of the IDEs in a given simchannel
          for(size_t iv = 0; iv < idevec.size(); ++iv){
            if (plist) {
              if(plist->find( idevec[iv].trackID ) == plist->end()
                  && idevec[iv].trackID != sim::NoParticleId)
              {
                mf::LogWarning("LArG4Ana") << idevec[iv].trackID << " is not in particle list";
              }
            }
            if(idevec[iv].trackID < 0) continue;
            totalCharge +=idevec[iv].numElectrons;
            scCharge += idevec[iv].numElectrons;
            totalEnergy +=idevec[iv].energy;
            scEnergy += idevec[iv].energy;

            totalEnergy_track[idevec[iv].trackID] += idevec[iv].energy/3.;
          }
        }
      }
     }
    }//End of if(fMakeLightAnalysisTree)


    if(!fUseLitePhotons)
    {

      //Reset counters
      fCountEventAll=0;
      fCountEventDetected=0;

      //Get *ALL* SimPhotonsCollection from Event
      std::vector< art::Handle< std::vector< sim::SimPhotons > > > photon_handles;
      evt.getManyByType(photon_handles);
      if (photon_handles.size() == 0)
        throw art::Exception(art::errors::ProductNotFound)<<"sim SimPhotons retrieved and you requested them.";

      for(auto const& mod : fInputModule){
      // sim::SimPhotonsCollection TheHitCollection = sim::SimListUtils::GetSimPhotonsCollection(evt,mod);
      //switching off to add reading in of labelled collections: Andrzej, 02/26/19

        for (auto const& ph_handle: photon_handles) {
          // Do some checking before we proceed
          if (!ph_handle.isValid()) continue;
          if (ph_handle.provenance()->moduleLabel() != mod) continue;   //not the most efficient way of doing this, but preserves the logic of the module. Andrzej

          bool Reflected = (ph_handle.provenance()->productInstanceName() == "Reflected");

          if((*ph_handle).size()>0)
          {
            if(fMakeLightAnalysisTree) {
            //resetting the signalt to save in the analysis tree per event
              const int maxNtracks = 1000;
              for(size_t itrack=0; itrack!=maxNtracks; itrack++) {
                for(size_t pmt_i=0; pmt_i!=geo->NOpChannels(); pmt_i++) {
                  fSignals_vuv[itrack][pmt_i].clear();
                  fSignals_vis[itrack][pmt_i].clear();
                }
              }
            }
          }


  //      if(fVerbosity > 0) std::cout<<"Found OpDet hit collection of size "<< TheHitCollection.size()<<std::endl;
          if(fVerbosity > 0) std::cout<<"Found OpDet hit collection of size "<< (*ph_handle).size()<<std::endl;

          if((*ph_handle).size()>0)
          {
  //           for(sim::SimPhotonsCollection::const_iterator itOpDet=TheHitCollection.begin(); itOpDet!=TheHitCollection.end(); itOpDet++)
            for(auto const& itOpDet: (*ph_handle) )
            {
              //Reset Counters
              fCountOpDetAll=0;
              fCountOpDetDetected=0;
              fCountOpDetReflDetected=0;
              //Reset t0 for visible light
              fT0_vis = 999.;

              //Get data from HitCollection entry
              fOpChannel=itOpDet.OpChannel();
              const sim::SimPhotons& TheHit=itOpDet;

              //std::cout<<"OpDet " << fOpChannel << " has size " << TheHit.size()<<std::endl;

              // Loop through OpDet phots.
              //   Note we make the screen output decision outside the loop
              //   in order to avoid evaluating large numbers of unnecessary
              //   if conditions.


              for(const sim::OnePhoton& Phot: TheHit)
              {
                // Calculate wavelength in nm
                fWavelength= odresponse->wavelength(Phot.Energy);

                //Get arrival time from phot
                fTime= Phot.Time;

                // special case for LibraryBuildJob: no working "Reflected" handle and all photons stored in single object - must sort using wavelength instead
                if(fPVS->IsBuildJob() && !Reflected) {
                  // all photons contained in object with Reflected = false flag
                  // Increment per OpDet counters and fill per phot trees
                  fCountOpDetAll++;
                  if(fMakeAllPhotonsTree){
                    if (!isVisible(fWavelength) || fPVS->StoreReflected()) {
                        fThePhotonTreeAll->Fill();
                    }
                  }

                  if(odresponse->detected(fOpChannel, Phot))
                  {
                    if(fMakeDetectedPhotonsTree) fThePhotonTreeDetected->Fill();
                    //only store direct direct light
                    if(!isVisible(fWavelength))
                      fCountOpDetDetected++;
                    // reflected and shifted light is in visible range
                    else if(fPVS->StoreReflected()) {
                      fCountOpDetReflDetected++;
                      // find the first visible arrival time
                      if(fPVS->StoreReflT0() && fTime < fT0_vis)
                        fT0_vis = fTime;
                    }
                    if(fVerbosity > 3)
                      std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 1 "<<std::endl;
                  }
                  else {
                    if(fVerbosity > 3)
                      std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 0 "<<std::endl;
                  }

                } // if build library and not reflected

                else {
                  // store in appropriate trees using "Reflected" handle and fPVS->StoreReflected() flag
                  // Increment per OpDet counters and fill per phot trees
                  fCountOpDetAll++;
                  if(fMakeAllPhotonsTree){
                    if (!Reflected || (fPVS->StoreReflected() && Reflected)) {
                        fThePhotonTreeAll->Fill();
                    }
                  }

                  if(odresponse->detected(fOpChannel, Phot))
                  {
                    if(fMakeDetectedPhotonsTree) fThePhotonTreeDetected->Fill();
                    //only store direct direct light
                    if(!Reflected)
                      fCountOpDetDetected++;
                    // reflected and shifted light is in visible range
                    else if(fPVS->StoreReflected() && Reflected ) {
                      fCountOpDetReflDetected++;
                      // find the first visible arrival time
                      if(fPVS->StoreReflT0() && fTime < fT0_vis)
                        fT0_vis = fTime;
                    }
                    if(fVerbosity > 3)
                      std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 1 "<<std::endl;
                  }
                  else {
                    if(fVerbosity > 3)
                      std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 0 "<<std::endl;
                  }
                }
              } // for each photon in collection



              // If this is a library building job, fill relevant entry
              if(fPVS->IsBuildJob() && !Reflected) // for library build job, both componenents stored in first object with Reflected = false
              {
                storeVisibility(fOpChannel, fCountOpDetDetected, fCountOpDetReflDetected, fT0_vis);
              }

              // Incremenent per event and fill Per OpDet trees
              if(fMakeOpDetsTree) fTheOpDetTree->Fill();
              fCountEventAll+=fCountOpDetAll;
              fCountEventDetected+=fCountOpDetDetected;

              // Give per OpDet output
              if(fVerbosity >2) std::cout<<"OpDetResponseInterface PerOpDet : Event "<<fEventID<<" OpDet " << fOpChannel << " All " << fCountOpDetAll << " Det " <<fCountOpDetDetected<<std::endl;
            }

            // Fill per event tree
            if(fMakeOpDetEventsTree) fTheEventTree->Fill();

            // Give per event output
            if(fVerbosity >1) std::cout<<"OpDetResponseInterface PerEvent : Event "<<fEventID<<" All " << fCountOpDetAll << " Det " <<fCountOpDetDetected<<std::endl;

          }
          else
          {
            // if empty OpDet hit collection,
            // add an empty record to the per event tree
            if(fMakeOpDetEventsTree) fTheEventTree->Fill();
          }
          if(fMakeLightAnalysisTree) {
            assert(mcpartVec);
            assert(fLightAnalysisTree);

            std::cout<<"Filling the analysis tree"<<std::endl;
            //---------------Filling the analysis tree-----------:
            fRun = evt.run();
            std::vector<double> this_xyz;

            //loop over the particles
            for(simb::MCParticle const& pPart: *mcpartVec){

              if(pPart.Process() == "primary")
                fEnergy = pPart.E();

              //resetting the vectors
              fstepPositions.clear();
              fstepTimes.clear();
              fSignalsvuv.clear();
              fSignalsvis.clear();
              fdEdx = -1.;
              //filling the tree fields
              fTrackID = pPart.TrackId();
              fpdg = pPart.PdgCode();
              fmotherTrackID = pPart.Mother();
              fdEdx = totalEnergy_track[fTrackID];
              fSignalsvuv = fSignals_vuv[fTrackID];
              fSignalsvis = fSignals_vis[fTrackID];
              fProcess = pPart.Process();
              //filling the center positions of each step
              for(size_t i_s=1; i_s < pPart.NumberTrajectoryPoints(); i_s++){
                this_xyz.clear();
                this_xyz.resize(3);
                this_xyz[0] = pPart.Position(i_s).X();
                this_xyz[1] = pPart.Position(i_s).Y();
                this_xyz[2] = pPart.Position(i_s).Z();
                fstepPositions.push_back(this_xyz);
                fstepTimes.push_back(pPart.Position(i_s).T());
              }
              //filling the tree per track
              fLightAnalysisTree->Fill();
            }
          } // if fMakeLightAnalysisTree
        }
      }
    }
    if (fUseLitePhotons)
    {

      //Get *ALL* SimPhotonsCollection from Event
      std::vector< art::Handle< std::vector< sim::SimPhotonsLite > > > photon_handles;
      evt.getManyByType(photon_handles);
      if (photon_handles.size() == 0)
        throw art::Exception(art::errors::ProductNotFound)<<"sim SimPhotons retrieved and you requested them.";

     //Get SimPhotonsLite from Event
     for(auto const& mod : fInputModule){
      //art::Handle< std::vector<sim::SimPhotonsLite> > photonHandle;
      //evt.getByLabel(mod, photonHandle);

        // Loop over direct/reflected photons
        for (auto const& ph_handle: photon_handles) {
          // Do some checking before we proceed
          if (!ph_handle.isValid()) continue;
          if (ph_handle.provenance()->moduleLabel() != mod) continue;   //not the most efficient way of doing this, but preserves the logic of the module. Andrzej

          bool Reflected = (ph_handle.provenance()->productInstanceName() == "Reflected");

          //Reset counters
          fCountEventAll=0;
          fCountEventDetected=0;

          if(fVerbosity > 0) std::cout<<"Found OpDet hit collection of size "<< (*ph_handle).size()<<std::endl;


          if((*ph_handle).size()>0)
          {

            for ( auto const& photon : (*ph_handle) )
            {
              //Get data from HitCollection entry
              fOpChannel=photon.OpChannel;
              std::map<int, int> PhotonsMap = photon.DetectedPhotons;

              //Reset Counters
              fCountOpDetAll=0;
              fCountOpDetDetected=0;
              fCountOpDetReflDetected=0;

              for(auto it = PhotonsMap.begin(); it!= PhotonsMap.end(); it++)
              {
                // Calculate wavelength in nm
                if (Reflected) {
                  fWavelength = kVisibleThreshold;
                }
                else {
                  fWavelength= kVUVWavelength;   // original
                }

                //Get arrival time from phot
                fTime= it->first;
                //std::cout<<"Arrival time: " << fTime<<std::endl;

                for(int i = 0; i < it->second ; i++)
                {
                  // Increment per OpDet counters and fill per phot trees
                  fCountOpDetAll++;
                  if(fMakeAllPhotonsTree) fThePhotonTreeAll->Fill();

                  if(odresponse->detectedLite(fOpChannel))
                  {
                    if(fMakeDetectedPhotonsTree) fThePhotonTreeDetected->Fill();
                    // direct light
                    if (!Reflected){
                      fCountOpDetDetected++;
                    }
                    else if (Reflected) {
                      fCountOpDetReflDetected++;
                    }
                    if(fVerbosity > 3)
                    std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 1 "<<std::endl;
                  }
                  else
                    if(fVerbosity > 3)
                    {
                    std::cout<<"OpDetResponseInterface PerPhoton : Event "<<fEventID<<" OpChannel " <<fOpChannel << " Wavelength " << fWavelength << " Detected 0 "<<std::endl;
                    }
                }
              }



              // Incremenent per event and fill Per OpDet trees
              if(fMakeOpDetsTree) fTheOpDetTree->Fill();
              fCountEventAll+=fCountOpDetAll;
              fCountEventDetected+=fCountOpDetDetected;

              if (fPVS->IsBuildJob())
                storeVisibility(fOpChannel, fCountOpDetDetected, fCountOpDetReflDetected, fT0_vis);


              // Give per OpDet output
              if(fVerbosity >2) std::cout<<"OpDetResponseInterface PerOpDet : Event "<<fEventID<<" OpDet " << fOpChannel << " All " << fCountOpDetAll << " Det " <<fCountOpDetDetected<<std::endl;
            }
            // Fill per event tree
            if(fMakeOpDetEventsTree) fTheEventTree->Fill();

            // Give per event output
            if(fVerbosity >1) std::cout<<"OpDetResponseInterface PerEvent : Event "<<fEventID<<" All " << fCountOpDetAll << " Det " <<fCountOpDetDetected<<std::endl;

          }
          else
          {
            // if empty OpDet hit collection,
            // add an empty record to the per event tree
            if(fMakeOpDetEventsTree) fTheEventTree->Fill();
          }
        }
      }
    }
  } // SimPhotonCounter::analyze()


  // ---------------------------------------------------------------------------
  void SimPhotonCounter::storeVisibility(
    int channel, int nDirectPhotons, int nReflectedPhotons,
    double reflectedT0 /* = 0.0 */
    ) const
  {
    phot::PhotonVisibilityService& pvs
      = *(art::ServiceHandle<phot::PhotonVisibilityService>());

    // ask PhotonVisibilityService which voxel was being served,
    // and how many photons where there generated (yikes!!);
    // this value was put there by LightSource (yikes!!)
    int VoxID;
    double NProd;
    fPVS->RetrieveLightProd(VoxID, NProd);

    pvs.SetLibraryEntry(VoxID, channel, double(nDirectPhotons)/NProd);

    //store reflected light
    if(fPVS->StoreReflected()) {
      pvs.SetLibraryEntry(VoxID, channel, double(nReflectedPhotons)/NProd, true);

      //store reflected first arrival time
      if (fPVS->StoreReflT0())
        pvs.SetLibraryReflT0Entry(VoxID, channel, reflectedT0);

    } // if reflected

  } // SimPhotonCounter::storeVisibility()

  // ---------------------------------------------------------------------------

}
namespace opdet{

  DEFINE_ART_MODULE(SimPhotonCounter)

}//end namespace opdet
