// -*- C++ -*-
//System includes

//Rivet framework
#include "Rivet/Analysis.hh"
#include "Rivet/RivetAIDA.hh"
#include "Rivet/Tools/Logging.hh"

//Projections
#include "Rivet/Projections/FinalState.hh"
#include "Rivet/Projections/WFinder.hh"
#include "Rivet/Projections/FastJets.hh"
//#include "Rivet/Projections/MissingMomentum.hh"

// Histogram booking
#include "LWH/Histogram1D.h"

namespace Rivet {

  /// Generic analysis looking at various distributions of final state particles
  class MC_GENSTUDY_JETCHARGE : public Analysis {
  public:

    /// Constructor
    MC_GENSTUDY_JETCHARGE()
      : Analysis("MC_GENSTUDY_JETCHARGE")
    {    }


  public:

    /// @name Analysis methods
    //@{

    /// Book histograms and initialise projections before the run
    void init() {

      // Projections
      const FinalState fs;
      addProjection(fs, "FS");
      //std::vector<std::pair<double, double > > electronEtaRanges;
      std::vector<std::pair<double, double > > muonEtaRanges;
      //electronEtaRanges.push_back(make_pair(-2.47,-1.37));
      //electronEtaRanges.push_back(make_pair(1.52,2.47));
      muonEtaRanges.push_back(make_pair(-2.4,2.4));

      WFinder muWFinder(fs, muonEtaRanges, 20*GeV, MUON, 
			40*GeV,1000*GeV,25*GeV,0.4,true,false,80.4,true);


      // Tag a W in the event, focus on jets that don't come from the W decay.
      addProjection(muWFinder,"muWFinder");

      FastJets JetProjection(muWFinder.remainingFinalState(),FastJets::KT,0.7); //
      addProjection(JetProjection,"Jets");

      // @todo Implement the following histos properly
      // 1. Invariant mass of 2/3-jet system
      // 2. Inclusive jet spectrum


      // Histograms
      _histJetMult	= bookHistogram1D("JetMult", 7, -0.5, 6.5);
      //Jet Kinematics
      _histJetPt	= bookHistogram1D("JetPt"	, 100, 20, 50);
      _histJetE		= bookHistogram1D("JetE"	, 100, 0, 200);
      _histJetEta	= bookHistogram1D("JetEta"	, 50, -5, 5);
      _histJetRapidity	= bookHistogram1D("JetRapidity"	, 50, -5, 5);
      _histJetPhi	= bookHistogram1D("JetPhi"	, 50, 0, TWOPI);
      _histJetMass      = bookHistogram1D("JetMass"	, 200, 0, 40);
      // Jet theoreticals
      _histJet2Mass	= bookHistogram1D("Jet2Mass"	, 200, 0, 100);
      _histJet3Mass	= bookHistogram1D("Jet3Mass"	, 200, 0, 100);
			
      _histSubJet2Mass	= bookHistogram1D("SubJet2Mass"	, 200, 0, 25);
      _histSubJet3Mass	= bookHistogram1D("SubJet3Mass"	, 200, 0, 25);
      _histSubJetDeltaR	= bookHistogram1D("SubJetDeltaR", 200, 0, 3.0);
      _histSubJetMass	= bookHistogram1D("SubJetMass"	, 200, 0, 10);
      _histSubJetSumEt	= bookHistogram1D("SubJetSumEt", 200, 0, 100);

      _histJetDipolarity= bookHistogram1D("JetDipolarity", 50, -2, 2);
      _histJetCharge    =  bookHistogram1D("JetCharge", 50, -0.5, 0.5);
    }
    /// Perform the per-event analysis
    void analyze(const Event& event) 
    {

      const WFinder& muWFinder = applyProjection<WFinder>(event,"muWFinder");
      if (muWFinder.bosons().size() != 1)
      {
	vetoEvent;
      }

      const double weight = event.weight();
      const FastJets& JetProjection = applyProjection<FastJets>(event, "Jets");
      const PseudoJets& jets = JetProjection.pseudoJetsByPt(20.0*GeV);

      if (jets.size() > 0) 
	{
	  unsigned int jetMult=jets.size();
	  _histJetMult->fill(jetMult);
	  ///This is dirty and lazy, but its the path of least resistance now!
	  ///FIXME: move into loop below to avoid iterating over jet collection twice!
	  /*
	  for(PseudoJets::const_iterator jet1=jets.begin(); jet1!=jets.end(); ++jet1)
	    {
	    }
	  */
	  //foreach(const fastjet::PseudoJet &jet, jets)
	  for(PseudoJets::const_iterator jet=jets.begin();jet!=jets.end();++jet)
	    {
	      //const FourMomentum& jetP=Rivet::momentum(jet);
	      if(jet->has_valid_cs())
		{
		  //check dcut==0.4 is appropriate
		  double dcut=0.4;
		  double sumEt=0.0;

		  PseudoJets subJets=jet->validated_cs()->exclusive_subjets(*jet,dcut);
		  unsigned int nSubJets=jet->validated_cs()->n_exclusive_subjets(*jet,dcut);

		  for(unsigned int j=0;j!=nSubJets;++j)
		    {
		      sumEt+=subJets.at(j).Et();
		      _histSubJetMass->fill(subJets.at(j).m());
		      for(unsigned int k=(j+1); k!=nSubJets;++k)
			{
			  _histSubJetDeltaR->fill(subJets.at(j).delta_R(subJets.at(k)),weight);
			  _histSubJet2Mass->fill((subJets.at(j)+subJets.at(k)).m(),weight);
			  for(unsigned int l=(k+1); l!=nSubJets; ++l)
			      _histSubJet2Mass->fill((subJets.at(j)+subJets.at(k)+subJets.at(l)).m(),weight);
			}
		    }
		  _histSubJetSumEt->fill(sumEt,weight);

		}
	      _histJetMass->fill(jet->m(),weight);
	      _histJetPt->fill(jet->pt(),weight);	
	      _histJetE->fill(jet->E(),weight);
	      _histJetEta->fill(jet->eta(),weight);	
	      _histJetRapidity->fill(jet->rapidity(),weight); 
	      _histJetPhi->fill(jet->phi(),weight);	
	      _histJetCharge->fill(JetProjection.JetCharge(*jet,0.5),weight);
	      _histJetDipolarity->fill(JetProjection.Dipolarity(*jet),weight);
	      
	      for(PseudoJets::const_iterator jet2=jet; jet2!=jets.end(); jet2++)
		{
		  _histJet2Mass->fill((*jet+*jet2).m(),weight);
		  for(PseudoJets::const_iterator jet3=jet2; jet3!=jets.end(); jet3++)
		    _histJet3Mass->fill((*jet+*jet2+*jet3).m(),weight);
		}
	    }
	}
      else
	vetoEvent;

    }
    /// Finalize
    void finalize() 
    {

      /*
      foreach(BookedHistos::value_type H,Histograms)
	scale(H.second,1/sumOfWeights());
      */

      // Jet Kinematics
      scale( _histJetMult ,1/sumOfWeights());
      scale( _histJetPt ,1/sumOfWeights());
      scale( _histJetE ,1/sumOfWeights());
      scale( _histJetEta ,1/sumOfWeights());
      scale( _histJetRapidity ,1/sumOfWeights());
      scale( _histJetPhi ,1/sumOfWeights());
      scale( _histJetMass ,1/sumOfWeights());


      // Jet Theoreticals 	
      scale( _histJet2Mass ,1/sumOfWeights());
      scale( _histJet3Mass ,1/sumOfWeights());

      scale( _histSubJet2Mass ,1/sumOfWeights());
      scale( _histSubJet3Mass ,1/sumOfWeights());
      scale( _histSubJetDeltaR ,1/sumOfWeights());
      scale( _histSubJetMass ,1/sumOfWeights());
      scale( _histSubJetSumEt ,1/sumOfWeights());
      scale(_histJetDipolarity,1/sumOfWeights());
      scale(_histJetCharge ,1/sumOfWeights());
    }
    //@}
  private:

    ///@param Histograms Indexed by histogram name for easy management
    ///until Rivet Autobooking takes over, allows any number of
    ///histograms to be added "on the fly" in the init() method.
    //@{
    AIDA::IHistogram1D *_histJetMult;

    //Jet Kinematics	
    AIDA::IHistogram1D *_histJetPt;
    AIDA::IHistogram1D *_histJetE;
    AIDA::IHistogram1D *_histJetEta;
    AIDA::IHistogram1D *_histJetRapidity;
    AIDA::IHistogram1D *_histJetPhi;
    AIDA::IHistogram1D *_histJetMass;

    // Jet theoreticals
    AIDA::IHistogram1D *_histJet2Mass;
    AIDA::IHistogram1D *_histJet3Mass;

    AIDA::IHistogram1D *_histSubJet2Mass;
    AIDA::IHistogram1D *_histSubJet3Mass;
    AIDA::IHistogram1D *_histSubJetDeltaR;
    AIDA::IHistogram1D *_histSubJetMass;
    AIDA::IHistogram1D *_histSubJetSumEt;

    AIDA::IHistogram1D *_histJetDipolarity;
    AIDA::IHistogram1D *_histJetCharge;    

    //@}
			


  };


  // The hook for the plugin system
  DECLARE_RIVET_PLUGIN(MC_GENSTUDY_JETCHARGE);

}