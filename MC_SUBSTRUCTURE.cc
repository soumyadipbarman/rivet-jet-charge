// -*- C++ -*-

#include <Rivet/Analysis.hh>
#include <Rivet/RivetAIDA.hh>
#include <Rivet/Tools/Logging.hh>
#include <Rivet/Projections/FinalState.hh>
#include <Rivet/Projections/FastJets.hh>

namespace Rivet {


  class MC_SUBSTRUCTURE : public Analysis {
  public:

    MC_SUBSTRUCTURE()
      : Analysis("MC_SUBSTRUCTURE")
    {    }

  public:

    double jetWidth(const Jet& jet) {
      double phi_jet = jet.phi();
      double eta_jet = jet.eta();
      double width = 0.0;
      double pTsum = 0.0;
      foreach (const Particle& p, jet.particles()) {
        double pT = p.momentum().pT();
        double eta = p.momentum().eta();
        double phi = p.momentum().phi();
        width += sqrt(pow(phi_jet - phi,2) + pow(eta_jet - eta ,2)) * pT;
        pTsum += pT;
      }
      if(pTsum != 0)return width/pTsum;
      else return 0.0;
    }

    // This is the code for the eccentricity calculation, copied and adapted from Lily's code
    double getEcc(const Jet& jet) {

      vector<double> phis;
      vector<double> etas;
      vector<double> energies;

      double etaSum = 0.; double phiSum = 0.; double eTot = 0.; 
      foreach (const Particle& p, jet.particles()) {

        double E = p.momentum().E();
        double eta = p.momentum().eta();
        
        energies.push_back(E);
        etas.push_back(jet.momentum().eta() - eta);
        
        eTot   += E;
        etaSum += eta * E;

        double dPhi = jet.momentum().phi() - p.momentum().phi();
        //if DPhi does not lie within 0 < DPhi < PI take 2*PI off DPhi
        //this covers cases where DPhi is greater than PI
        if( fabs( dPhi - TWOPI ) < fabs(dPhi) ) dPhi -= TWOPI;
        //if DPhi does not lie within -PI < DPhi < 0 add 2*PI to DPhi
        //this covers cases where DPhi is less than -PI
        else if( fabs(dPhi + TWOPI) < fabs(dPhi) ) dPhi += TWOPI;
       
        phis.push_back(dPhi);
        
        phiSum += dPhi * E;
      }

      //these are the "pull" away from the jet axis
      if(eTot != 0){	etaSum = etaSum/eTot; 
      			phiSum = phiSum/eTot;
		   }
      else 	   {	etaSum = 0; phiSum = 0;}

      // now for every cluster we alter its position by moving it:
      // away from the new axis if it is in the direction of -ve pull
      // closer to the new axis if it is in the direction of +ve pull
      // the effect of this will be that the new energy weighted center will be on the old jet axis. 
      double little_x=0., little_y=0.;
      for(unsigned int k = 0; k< jet.particles().size(); k++) {
        little_x+= etas[k]-etaSum;
        little_y+= phis[k]-phiSum;

        etas[k] = etas[k]-etaSum;
        phis[k] = phis[k]-phiSum;
     }

      double X1=0.; 
      double X2=0.; 
      for(unsigned int i = 0; i < jet.particles().size(); i++) {
        X1 += 2. * energies[i]* etas[i] * phis[i]; // this is =2*X*Y
        X2 += energies[i]*(phis[i] * phis[i] - etas[i] * etas[i] ); // this isX^2 - Y^2
      }
      
        // variance calculations 
      double Theta = .5*atan2(X1,X2);

      double sinTheta =sin(Theta);
      double cosTheta = cos(Theta);
      double Theta2 = Theta + 0.5*PI;
      double sinThetaPrime = sin(Theta2);
      double cosThetaPrime = cos(Theta2);

      double VarX = 0.;
      double VarY = 0.;
      for(unsigned int i = 0; i < jet.particles().size(); i++) {
        double X=sinTheta*etas[i] + cosTheta*phis[i];    
        double Y=sinThetaPrime*etas[i] + cosThetaPrime*phis[i]; 
        VarX += energies[i]*X*X;
        VarY += energies[i]*Y*Y;
      }
       
      double VarianceMax = VarX; 
      double VarianceMin = VarY;
     
      if(VarianceMax < VarianceMin) {
        VarianceMax = VarY;
        VarianceMin = VarX;
      }
      
      double ECC;
      if(VarianceMax != 0)ECC=1.0 - (VarianceMin/VarianceMax);
      else ECC = 0;
      return ECC;
    }

    // This is the code for the planar flow calculation, copied and adapted from Lily's code
    double getPFlow(const  Jet& jet) {
      double phi0=jet.momentum().phi();
      double eta0=jet.momentum().eta();

      double nref[3];
      if(cosh(eta0) != 0)nref[0]=(cos(phi0)/cosh(eta0));
      else nref[0] = 0;
      if(cosh(eta0) != 0)nref[1]=(sin(phi0)/cosh(eta0));
      else nref[1] = 0;
      nref[2]=tanh(eta0);
      
      // This is the rotation matrix
      double RotationMatrix[3][3];
      CalcRotationMatrix(nref, RotationMatrix);
      
      double Iw00(0.), Iw01(0.), Iw11(0.), Iw10(0.);
     
      foreach (const Particle& p, jet.particles()) {
        if(p.momentum().E()*jet.momentum().mass() == 0.)continue;
        double a=1./(p.momentum().E()*jet.momentum().mass());
        FourMomentum rotclus = RotateAxes(p.momentum(), RotationMatrix);
        Iw00 += a*pow(rotclus.px(), 2);
        Iw01 += a*rotclus.px()*rotclus.py();
        Iw10 += a*rotclus.py()*rotclus.px();
        Iw11 += a*pow(rotclus.py(), 2);
      }

      double det=Iw00*Iw11-Iw01*Iw10;
      double trace=Iw00+Iw11;
      double pf;
      if(trace != 0)pf=(4.0*det)/(pow(trace,2));
      else pf = 0;

      return pf;
    }
   

    // This is the code for the angularity calculation, copied and adapted from Lily's code
    double getAngularity(const Jet& jet) {
      double sum_a=0.;
      //This a used in angularity calc can take any value <2 (e.g. 1,0,-0.5 etc) for infrared safety
      const double a=-2.;
     
      foreach (const Particle& p, jet.particles()) {
        double e_i       = p.momentum().E();
        double theta_i   = jet.momentum().angle(p.momentum());
	double e_theta_i;
	if(sin(theta_i) == 0.) e_theta_i = 0;
        else e_theta_i = e_i * pow(sin(theta_i),a) * pow(1-cos(theta_i),1-a);
        sum_a += e_theta_i;
      }

      double Angularity;

      if(jet.momentum().mass() != 0)Angularity = sum_a/jet.momentum().mass();//mass is in MeV
      else Angularity = 0.0;
      return Angularity;
    }

    // Adapted code from Lily  
    FourMomentum RotateAxes(const Rivet::FourMomentum& p, double M[3][3]){
      double px_rot=M[0][0]*(p.px())+M[0][1]*(p.py())+M[0][2]*(p.pz());
      double py_rot=M[1][0]*(p.px())+M[1][1]*(p.py())+M[1][2]*(p.pz());
      double pz_rot=M[2][0]*(p.px())+M[2][1]*(p.py())+M[2][2]*(p.pz()); 
      return FourMomentum(p.E(), px_rot, py_rot, pz_rot);
    } 
    
    // Untouched code from Lily  
    void CalcRotationMatrix(double nvec[3],double rot_mat[3][3]){
      //clear momentum tensor
      for(int i=0; i<3; i++){
        for(int j=0; j<3; j++){
          rot_mat[i][j]=0.;
        }
      }
      double mag3=sqrt(nvec[0]*nvec[0] + nvec[1]*nvec[1]+ nvec[2]*nvec[2]);
      double mag2=sqrt(nvec[0]*nvec[0] + nvec[1]*nvec[1]);
      if(mag3<=0){
        cout<<"rotation axis is null"<<endl;
        return;
      }

      double ctheta0=nvec[2]/mag3;
      double stheta0=mag2/mag3;
      double cphi0 = (mag2>0.) ? nvec[0]/mag2:0.;
      double sphi0 = (mag2>0.) ? nvec[1]/mag2:0.;

      rot_mat[0][0]=-ctheta0*cphi0;
      rot_mat[0][1]=-ctheta0*sphi0;
      rot_mat[0][2]=stheta0;
      rot_mat[1][0]=sphi0;
      rot_mat[1][1]=-1.*cphi0;
      rot_mat[1][2]=0.;
      rot_mat[2][0]=stheta0*cphi0;
      rot_mat[2][1]=stheta0*sphi0;
      rot_mat[2][2]=ctheta0;

      return;
    }

    void init() {

      FinalState fs(-4.0, 4.0, 0*GeV);
      addProjection(fs, "FS");
      addProjection(FastJets(fs, FastJets::CAM, 1.2), "Jets");

	//stuff from adapted code, ungroomed mass

      _h_njets = bookHistogram1D("njets", 11, 0, 11);
      _h_jetmass = bookHistogram1D("jetmass", 50, 0, 250);
      _h_ecc = bookHistogram1D("Eccentricity", 50, 0, 1);
      _h_width = bookHistogram1D("Width", 50, 0, 1);
      _h_pflow = bookHistogram1D("PFlow", 50, 0, 1);
      _h_angularity = bookHistogram1D("Angularity", 50, 0, 0.1);

	//grooming histos

      _h_FiltMass = bookHistogram1D("Filtered_mass", 50, 0, 250);
      _h_TrimMass = bookHistogram1D("Trimmed_mass", 50, 0, 250);
      _h_PrunMass = bookHistogram1D("Pruned_mass", 50, 0, 250);

	//n-subjettiness histos	

      _h_32subjet = bookHistogram1D("Tau_32", 50, 0, 1.2);
      _h_21subjet = bookHistogram1D("Tau_21", 50, 0, 1.2);
      _h_3subjet = bookHistogram1D("Tau_3", 50, 0, 1);
      _h_2subjet = bookHistogram1D("Tau_2", 50, 0, 1);
      _h_1subjet = bookHistogram1D("Tau_1", 50, 0, 1);

	//ASF histos

      _h_ASF_1peak_m = bookHistogram1D("1_peak_m", 50, 0, 200);
      _h_ASF_1peak_r = bookHistogram1D("1_peak_r", 50, 0, 2);
      _h_ASF_2peak_m1 = bookHistogram1D("2_peak_m1", 50, 0, 200);
      _h_ASF_2peak_r1 = bookHistogram1D("2_peak_r1", 50, 0, 2);
      _h_ASF_2peak_m2 = bookHistogram1D("2_peak_m2", 50, 0, 200);
      _h_ASF_2peak_r2 = bookHistogram1D("2_peak_r2", 50, 0, 2);
      _h_ASF_3peak_m1 = bookHistogram1D("3_peak_m1", 50, 0, 200);
      _h_ASF_3peak_r1 = bookHistogram1D("3_peak_r1", 50, 0, 2);
      _h_ASF_3peak_m2 = bookHistogram1D("3_peak_m2", 50, 0, 200);
      _h_ASF_3peak_r2 = bookHistogram1D("3_peak_r2", 50, 0, 2);
      _h_ASF_3peak_m3 = bookHistogram1D("3_peak_m3", 50, 0, 200);
      _h_ASF_3peak_r3 = bookHistogram1D("3_peak_r3", 50, 0, 2);
      _h_npeaks = bookHistogram1D("npeaks", 100, 0, 5);

    }

    void analyze(const Event& event) {
      const double weight = event.weight();

	//Require p_T > 350 GeV and 140 GeV < m_J < 250 GeV to make sure we are mainly looking
	//at boosted tops. Only take two highest p_T jets which satisfy requirements.
      Jets ajets = applyProjection<JetAlg>(event, "Jets").jetsByPt(350*GeV);
      Jets jets;
      foreach (const Jet& aj, ajets) {
	if (jets.size() > 1) break;
        if (aj.momentum().mass()/GeV > 140 && aj.momentum().mass()/GeV < 250) jets.push_back(aj);
      }

      _h_njets->fill(jets.size(), weight);

	//Plot eccentricity etc
      foreach(const Jet j, jets) {
        _h_ecc->fill(getEcc(j), weight);
        _h_width->fill(jetWidth(j), weight);
        _h_angularity->fill(getAngularity(j), weight);
        _h_pflow->fill(getPFlow(j), weight);
        _h_jetmass->fill(j.momentum().mass()/GeV, weight);
      }

      using namespace fastjet;
      const FastJets& fj = applyProjection<FastJets>(event, "Jets");
      const PseudoJets apsjets = fj.pseudoJetsByPt(350*GeV);
      PseudoJets psjets;
      foreach (const PseudoJet ajet, apsjets) {
	if (psjets.size() > 1) break;
	if (ajet.m() > 140 && ajet.m() < 250) psjets.push_back(ajet);
      }

      // Grooming algorithms
      foreach (const PseudoJet pjet, psjets) {
	_h_FiltMass->fill(fj.Filter(pjet, FastJets::CAM, 3, 0.3).m(), weight);
	_h_TrimMass->fill(fj.Trimmer(pjet, FastJets::CAM, 0.03, 0.3).m(), weight);
	_h_PrunMass->fill(fj.Pruner(pjet, FastJets::CAM, 0.1, pjet.m()/pjet.pt()).m(), weight);
      }

      //N-subjettiness, use beta = 1 since dealing with tops (and for simplifying
      //minimisation procedure)
      foreach (const PseudoJet pjet, psjets) {
	PseudoJets constituents = pjet.constituents();
	if(constituents.size() < 3) continue;
	PseudoJets axis1 = fj.GetAxes(1, constituents, FastJets::KT, M_PI/2.0);
	PseudoJets axis2 = fj.GetAxes(2, constituents, FastJets::KT, M_PI/2.0);
	PseudoJets axis3 = fj.GetAxes(3, constituents, FastJets::KT, M_PI/2.0);
	//Lloyd algorithm for local minimum, only need one run since beta = 1
	fj.UpdateAxes(1, constituents, axis1);
	fj.UpdateAxes(1, constituents, axis2);
	fj.UpdateAxes(1, constituents, axis3);
	//plot Tau values
	double tau1 = fj.TauValue(1, 1.2, constituents, axis1);
	double tau2 = fj.TauValue(1, 1.2, constituents, axis2);
	double tau3 = fj.TauValue(1, 1.2, constituents, axis3);
	_h_1subjet->fill(tau1, weight);
	_h_2subjet->fill(tau2, weight);
	_h_3subjet->fill(tau3, weight);
	if(tau1 != 0)_h_21subjet->fill(tau2/tau1, weight);
	if(tau2 != 0)_h_32subjet->fill(tau3/tau2, weight);
      }

	//ASF peaks
      foreach (const PseudoJet pjet, psjets) {
	PseudoJets constituents = pjet.constituents();
	if (constituents.size() < 3) continue;
	//require min prominence = 4.0
	vector<ACFpeak> peaks = fj.ASFPeaks(constituents, 0, 4.0);
	_h_npeaks->fill(peaks.size(), weight);
	if(peaks.size() == 1) {
	  _h_ASF_1peak_m->fill(peaks[0].partialmass, weight);
	  _h_ASF_1peak_r->fill(peaks[0].Rval, weight);
	}
	if(peaks.size() == 2) {
	  _h_ASF_2peak_m1->fill(peaks[0].partialmass, weight);
	  _h_ASF_2peak_r1->fill(peaks[0].Rval, weight);
	  _h_ASF_2peak_m2->fill(peaks[1].partialmass, weight);
	  _h_ASF_2peak_r2->fill(peaks[1].Rval, weight);
	}
	if(peaks.size() == 3) {
	  _h_ASF_3peak_m1->fill(peaks[0].partialmass, weight);
	  _h_ASF_3peak_r1->fill(peaks[0].Rval, weight);
	  _h_ASF_3peak_m2->fill(peaks[1].partialmass, weight);
	  _h_ASF_3peak_r2->fill(peaks[1].Rval, weight);
	  _h_ASF_3peak_m3->fill(peaks[2].partialmass, weight);
	  _h_ASF_3peak_r3->fill(peaks[2].Rval, weight);
	}
      }
    }


    /// Normalise histograms etc., after the run
    void finalize() {
      normalize(_h_njets);      
      normalize(_h_jetmass);
      normalize(_h_ecc);
      normalize(_h_width);
      normalize(_h_pflow);
      normalize(_h_angularity);

      normalize(_h_FiltMass);
      normalize(_h_TrimMass);
      normalize(_h_PrunMass);

      normalize(_h_21subjet);
      normalize(_h_32subjet);
      normalize(_h_1subjet);
      normalize(_h_2subjet);
      normalize(_h_3subjet);

      normalize(_h_ASF_1peak_m);
      normalize(_h_ASF_1peak_r);
      normalize(_h_ASF_2peak_m1);
      normalize(_h_ASF_2peak_r1);
      normalize(_h_ASF_2peak_m2);
      normalize(_h_ASF_2peak_r2);
      normalize(_h_ASF_3peak_m1);
      normalize(_h_ASF_3peak_r1);
      normalize(_h_ASF_3peak_m2);
      normalize(_h_ASF_3peak_r2);
      normalize(_h_ASF_3peak_m3);
      normalize(_h_ASF_3peak_r3);
      normalize(_h_npeaks);
    }


  private:

    AIDA::IHistogram1D *_h_njets, *_h_jetmass;
    AIDA::IHistogram1D *_h_ecc, *_h_width, *_h_pflow, *_h_angularity;

    AIDA::IHistogram1D *_h_FiltMass, *_h_TrimMass, *_h_PrunMass;

    AIDA::IHistogram1D *_h_3subjet, *_h_2subjet, *_h_1subjet, *_h_21subjet, *_h_32subjet;

    AIDA::IHistogram1D *_h_ASF_1peak_m, *_h_ASF_1peak_r, *_h_ASF_2peak_m1, 
	*_h_ASF_2peak_r1, *_h_ASF_2peak_m2, *_h_ASF_2peak_r2, *_h_ASF_3peak_m1, 
	*_h_ASF_3peak_r1, *_h_ASF_3peak_m2, *_h_ASF_3peak_r2, *_h_ASF_3peak_m3, 
	*_h_ASF_3peak_r3, *_h_npeaks;

  };

  // The hook for the plugin system
  DECLARE_RIVET_PLUGIN(MC_SUBSTRUCTURE);

}