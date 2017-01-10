#include "PLTAnalysis.h"

using namespace std;

PLTAnalysis::PLTAnalysis(string const inFileName, TFile * Out_f,  TString const runNumber, uint8_t const TelescopeID, bool TrackOnlyTelescope):
    telescopeID(TelescopeID), InFileName(inFileName), RunNumber(runNumber),
    now1(clock()), now2(clock()), loop(0), startProg(0), endProg(0), allProg(0), averTime(0),
    TimeWidth(20000), StartTime(0), NGraphPoints(0),
    PHThreshold(3e5), trackOnlyTelescope(TrackOnlyTelescope)
{
    out_f = Out_f;
    /** set up root */
    gStyle->SetOptStat(0);
    gErrorIgnoreLevel = kWarning;
    /** single plane studies */
    SinglePlaneStudies();
    /** init file reader */
    InitFileReader();
    if (GetUseRootInput(telescopeID)) nEntries = ((PSIRootFileReader*) FR)->fTree->GetEntries();
    stopAt = nEntries;
//    stopAt = 1e4;
    /** apply masking */
    FR->ReadPixelMask(GetMaskingFilename(telescopeID));
    /** init histos */
    Histos = new RootItems(telescopeID, RunNumber);
    cout << "Output directory: " << Histos->getOutDir() << endl;
    /** init file writer */
    if (UseFileWriter(telescopeID)) FW = new FileWriterTracking(InFileName, telescopeID, FR);
//    if (telescopeID == 7 || telescopeID == 8 || telescopeID == 9 || telescopeID == 10 || telescopeID >= 11) FW = new FileWriterTracking(InFileName, telescopeID, FR);
}

PLTAnalysis::~PLTAnalysis()
{
    FW->saveTree();
    delete FR;
    delete FW;
    delete Histos;
}


/** ============================
 EVENT LOOP
 =================================*/
 void PLTAnalysis::EventLoop(){

    getTime(now1, startProg);
    now1 = clock();
//    cout << "stopAt = " << stopAt << endl;
//        stopAt = 1e5;
    for (uint32_t ievent = 0; FR->GetNextEvent() >= 0; ++ievent) {
        if (ievent > stopAt) break;
        ThisTime = ievent;

//        cout << ievent << " ";
        MeasureSpeed(ievent);
        PrintProcess(ievent);
        /** file writer */
        if (GetUseRootInput(telescopeID)) WriteTrackingTree();

        /** fill coincidence map */
        Histos->CoincidenceMap()->Fill(FR->HitPlaneBits() );

        /** make average pulseheight maps*/
        if (ThisTime - (StartTime + NGraphPoints * TimeWidth) > TimeWidth)
            MakeAvgPH();

        /** draw tracks if there is more than one hit*/
        DrawTracks();

        /** loop over the planes */
        for (size_t iplane = 0; iplane != FR->NPlanes(); ++iplane) {

            PLTPlane * Plane = FR->Plane(iplane);
            /** Check that the each hit belongs to only one cluster type*/ //todo: DA: comentar
//    	    Plane->CheckDoubleClassification();
            /** fill cluster histo */
            Histos->nClusters()[Plane->ROC()]->Fill(Plane->NClusters());

            for (size_t icluster = 0; icluster != Plane->NClusters(); ++icluster) {
                PLTCluster* Cluster = Plane->Cluster(icluster);

                /** ignore charges above a certain threshold */
                if (Cluster->Charge() > PHThreshold) continue;

                /** fill pulse heigh histos */
                FillPHHistos(iplane, Cluster);

                /** fill hits per cluster histo */
                Histos->nHitsPerCluster()[Cluster->ROC()]->Fill(Cluster->NHits());

                /** fill high and low occupancy */
                FillOccupancyHiLo(Cluster);
            }
            /** fill occupancy histo */
            FillOccupancy(Plane);
        }


//        cout << "Number of Tracks: " << FR->NTracks() << endl;
        if (UseFileWriter(telescopeID) && FR->NTracks() == 1 && ((FR->Track(0)->NClusters() == Histos->NRoc()  && !trackOnlyTelescope) || (FR->Track(0)->NClusters() >= 4  && trackOnlyTelescope))){
//		if ((telescopeID == 7 || telescopeID == 8 || telescopeID == 9 || telescopeID >= 10) && FR->NTracks() == 1 && FR->Track(0)->NClusters() == Histos->NRoc() ){

		  do_slope = true;
		  for (uint8_t i_rocs(0); i_rocs != FR->Track(0)->NClusters(); i_rocs++)
		    if (FR->Track(0)->Cluster(i_rocs)->Charge() > PHThreshold){
		      do_slope = false;
		      break;
		    }

		  if (do_slope) {

		    PLTTrack * Track = FR->Track(0);
		    //				for (uint8_t i=0; i != FR->Signal().size(); i++)
		    //                    std::cout << FR->SignalDiamond(i) << " ";
		    //                std::cout << std::endl;

		    //                if (Track->NHits() == 4){
		    //                    for (uint16_t i = 0; i < Track->NClusters(); i++){
		    //                        cout << "Plane: " << i << ":\t" << setprecision(2) << setw(4) << Track->Cluster(i)->TX()*100 << "\t" << Track->Cluster(i)->TY()*100;
		    //                        cout << "\t" << Track->Cluster(i)->Hit(0)->Column() << "\t"<< Track->Cluster(i)->Hit(0)->Row() << endl;
		    //                    }
		    //                    cout << Track->fChi2X << " " << Track->fChi2Y << " " << Track->fChi2 << endl;
		    ////                    float y1 = Track->Cluster(2)->TY() - Track->Cluster(1)->TY();
		    ////                    float y2 = Track->Cluster(1)->TY() - Track->Cluster(0)->TY();
		    ////                    cout << y1*100 << " " << y2*100 << endl;
		    ////                    cout << (Track->Cluster(2)->TX() - Track->Cluster(1)->TX()) / (Track->Cluster(1)->TX() - Track->Cluster(0)->TX()) * 2.032 << " ";
		    ////                    cout << (Track->Cluster(2)->TY() - Track->Cluster(1)->TY()) / (Track->Cluster(1)->TY() - Track->Cluster(0)->TY()) * 2.032 << endl<<endl;
		    //
		    //                }

		    /** fill chi2 histos */
		    Histos->Chi2()->Fill(Track->Chi2());
		    Histos->Chi2X()->Fill(Track->Chi2X());
		    Histos->Chi2Y()->Fill(Track->Chi2Y());

		    /** fill slope histos */
		    Histos->TrackSlopeX()->Fill(Track->fAngleX);
		    Histos->TrackSlopeY()->Fill(Track->fAngleY);

		    //                if (ievent < 100){
		    //                    for (uint8_t iSig = 0; iSig != Track->NClusters(); iSig++)
		    //                        std::cout<< Track->Cluster(iSig)->TX() << " " << Track->Cluster(iSig)->TY() << " " << Track->Cluster(iSig)->TZ() << std::endl;
		    //                        std::cout << Track->fChi2X << " " << Track->fChi2Y << " " << Track->fChi2 << std::endl;
		    //                        std::cout << Track->fAngleRadX << " " << Track->fOffsetX << Track->fAngleRadY << " " << Track->fOffsetY << std::endl;
		    //                    std::cout << std::endl;
		    //                }

		    /** fill signal histos */
		    if (FillSignalHistos(telescopeID)) {
//		    if (telescopeID == 9 || telescopeID == 8 || telescopeID ==7) {
		      if (ievent > 0 && FW->InTree()->GetBranch(GetSignalBranchName())){
                        for (uint8_t iSig = 0; iSig != Histos->NSig(); iSig++){
              float dia1z = GetDiamondZPosition(telescopeID, 1);
              float dia2z = GetDiamondZPosition(telescopeID, 2);
			  if (iSig < 2)
			    Histos->SignalDisto()[iSig]->Fill(Track->ExtrapolateX(dia1z), Track->ExtrapolateY(dia1z), FR->SignalDiamond(iSig) );
			  else
			    Histos->SignalDisto()[iSig]->Fill(Track->ExtrapolateX(dia2z), Track->ExtrapolateY(dia2z), FR->SignalDiamond(iSig) );
                        }
		      }
		    }

		    /** loop over the clusters */
		    for (size_t icluster = 0; icluster < Track->NClusters(); icluster++){

					/** get the ROC in of the cluster and fill the corresponding residual */
		      uint8_t ROC = Track->Cluster(icluster)->ROC();
		      PLTCluster * Cluster = Track->Cluster(icluster);

		      /** fill residuals */
		      Histos->Residual()[ROC]->Fill(Track->LResidualX(ROC), Track->LResidualY(ROC)); // dX vs dY
					Histos->ResidualXdY()[ROC]->Fill(Cluster->LX(), Track->LResidualY(ROC));// X vs dY
					Histos->ResidualYdX()[ROC]->Fill(Cluster->LY(), Track->LResidualX(ROC)); // Y vs dX

					/** ignore events above a certain threshold */
					if (Cluster->Charge() > PHThreshold) continue;
					//printf("High Charge: %13.3E\n", Cluster->Charge());

					/** fill the offline pulse heights (Track6+|Slope| < 0.01 in x and y ) */
					FillOfflinePH(Track, Cluster);
		    }
		  }
		}


    } /** END OF EVENT LOOP */
    /** add the last point to the average pulse height graph */
    MakeAvgPH();

    cout << endl;
    getTime(now1, loop);
    now1 = clock();
 }
/** ============================
 AFTER LOOP -> FINISH
 =================================*/
 void PLTAnalysis::FinishAnalysis(){

    out_f->cd();

    Histos->SaveAllHistos();

    /** make index.html as overview */
	WriteHTML(Histos->getPlotsDir() + RunNumber, GetCalibrationFilename(telescopeID), telescopeID);

    getTime(now1, endProg);
    getTime(now2, allProg);
    /** print total events */
    cout << "=======================\n";
    cout << "Total events: " << nEntries << endl;
    cout << "Start       : " << setprecision(2) << fixed << startProg << " seconds\n";
    cout << "Loop        : " << setprecision(2) << fixed << loop << " seconds\n";
    cout << "End         : " << setprecision(2) << fixed << endProg << " seconds\n";
    cout << "All         : " << setprecision(2) << fixed << allProg << " seconds\n";
    cout <<"=======================\n";
}


/** ============================
 AUXILIARY FUNCTIONS
 =================================*/
void PLTAnalysis::SinglePlaneStudies(){

    if ((telescopeID == 1) || (telescopeID == 2)){
        int n_events = TestPlaneEfficiencySilicon(InFileName, out_f, RunNumber, telescopeID);
        for (uint8_t iplane = 1; iplane != 5; iplane++){
            cout << "Going to call TestPlaneEfficiency " << iplane << endl;
            TestPlaneEfficiency(InFileName, out_f, RunNumber, iplane, n_events, telescopeID);
        }
    }
}
void PLTAnalysis::InitFileReader(){

    if (GetUseRootInput(telescopeID)){
        FR = new PSIRootFileReader(InFileName, GetCalibrationFilename(telescopeID), GetAlignmentFilename(telescopeID),
            GetNumberOfROCS(telescopeID), GetUseGainInterpolator(telescopeID), GetUseExternalCalibrationFunction(telescopeID), false, telescopeID, trackOnlyTelescope);
    }
    else {
        FR = new PSIBinaryFileReader(InFileName, GetCalibrationFilename(telescopeID), GetAlignmentFilename(telescopeID),
            GetNumberOfROCS(telescopeID), GetUseGainInterpolator(telescopeID), GetUseExternalCalibrationFunction(telescopeID));
        ((PSIBinaryFileReader*) FR)->CalculateLevels(Histos->getOutDir());
    }
    FR->GetAlignment()->SetErrors(telescopeID);
    FILE * f = fopen("MyGainCal.dat", "w");
    FR->GetGainCal()->PrintGainCal(f);
    fclose(f);
}
float PLTAnalysis::getTime(float now, float & time){

    time += (clock() - now) / CLOCKS_PER_SEC;
    return time;
}
void PLTAnalysis::PrintProcess(uint32_t ievent){

    if (ievent == 0) cout << endl;
    if (ievent % 10 == 0 && ievent >= 1000){
        if (ievent != 1000) cout << "\x1B[A\r";
        cout << "Processed events:\t"  << setprecision(2) << setw(5) << setfill('0') << fixed << float(ievent) / stopAt * 100 << "% ";
        if ( stopAt - ievent < 10 ) cout << "|" <<string( 50 , '=') << ">";
        else cout << "|" <<string(int(float(ievent) / stopAt * 100) / 2, '=') << ">";
        if ( stopAt - ievent < 10 ) cout << "| 100%    ";
        else cout << string(50 - int(float(ievent) / stopAt * 100) / 2, ' ') << "| 100%    " << endl;
        float all_seconds = (stopAt - ievent) / speed;
        uint16_t minutes = all_seconds / 60;
        uint16_t seconds = all_seconds - int(all_seconds) / 60 * 60;
        uint16_t miliseconds =  (all_seconds - int(all_seconds)) * 1000;
//        if (speed) cout << "time left:\t\t" << setprecision(2) << fixed << (nEntries - ievent) / speed <<  " seconds     " << minutes;
        if (speed) {
            cout << "time left:\t\t" << setw(2) << setfill('0') << minutes;
            cout << ":" << setw(2) << setfill('0') << seconds;
            cout << ":" << setw(3) << setfill('0') << miliseconds << "      ";
        }
        //else cout << "time left: ???";// Don't know why it is persistent. Commented it :P
    }
}
void PLTAnalysis::MeasureSpeed(uint32_t ievent){

    if (ievent == 10000) now = clock();
    if (ievent % 10000 == 0 && ievent >= 20000){
        speed = (ievent - 10000) / getTime(now, averTime);
        now = clock();
    }
}
void PLTAnalysis::WriteTrackingTree(){

    /** first clear all vectors */
    FW->clearVectors();
    FW->setHitPlaneBits(FR->HitPlaneBits() );
    FW->setNTracks(FR->NTracks() );
    FW->setNClusters(FR->NClusters() );

    for (uint8_t iplane = 0; iplane != FR->NPlanes(); ++iplane) {
      PLTPlane * Plane = FR->Plane(iplane);
      FW->setNHits(iplane, Plane->NHits() );
    }
    if (FR->NTracks() > 0){
        PLTTrack * Track = FR->Track(0);
        FW->setChi2(Track->Chi2() );
        FW->setChi2X(Track->Chi2X() );
        FW->setChi2Y(Track->Chi2Y() );
        FW->setAngleX(Track->fAngleX);
        FW->setAngleY(Track->fAngleY);
        float dia1z = GetDiamondZPosition(telescopeID, 1);
        float dia2z = GetDiamondZPosition(telescopeID, 2);
        FW->setDia1TrackX(Track->ExtrapolateX(dia1z));
        FW->setDia1TrackY(Track->ExtrapolateY(dia1z));
        FW->setDia2TrackX(Track->ExtrapolateX(dia2z));
        FW->setDia2TrackY(Track->ExtrapolateY(dia2z));
        FW->setDistDia1(Track->ExtrapolateX(dia1z), Track->ExtrapolateY(dia1z));
        FW->setDistDia2(Track->ExtrapolateX(dia2z), Track->ExtrapolateY(dia2z));

        FW->setCoincidenceMap(FR->HitPlaneBits());
        for (uint8_t iplane = 0; iplane != FR->NPlanes(); ++iplane) {
            PLTTrack * Track2 = FR->Track(0);
            PLTPlane * Plane = FR->Plane(iplane);
            FW->setClusters(iplane, Plane->NClusters() );
            FW->setResidualsX(iplane, ((Plane->NClusters() == 1) ? Track2->LResidualX(iplane) : -999));
//            if (Plane->NHits() == 1 and iplane == 4){
//              cout << (Plane->Hit(0)->Column() == Plane->Cluster(0)->SeedHit()->Column()) << endl;
//            }
            for (size_t icluster = 0; icluster != Plane->NClusters(); icluster++) {
                FW->setClusterPlane(iplane);
                FW->setChargeAll(iplane, Plane->Cluster(icluster)->Charge());
                FW->setClusterSize(iplane, Plane->Cluster(icluster)->NHits());
                FW->setClusterPositionTelescopeX(iplane, Plane->Cluster(icluster)->TX() );
                FW->setClusterPositionTelescopeY(iplane, Plane->Cluster(icluster)->TY() );
                FW->setClusterPositionLocalX(iplane, Plane->Cluster(icluster)->LX() );
                FW->setClusterPositionLocalY(iplane, Plane->Cluster(icluster)->LY() );
                FW->setResidualLocalX(iplane, Track2->LResidualX(iplane));
                FW->setResidualLocalY(iplane, Track2->LResidualY(iplane));
                FW->setClusterRow(iplane, Plane->Cluster(icluster)->SeedHit()->Row() );
                FW->setClusterColumn(iplane, Plane->Cluster(icluster)->SeedHit()->Column() );
                FW->setTrackX(iplane, Track2->ExtrapolateX(Plane->GZ()));
                FW->setTrackY(iplane, Track2->ExtrapolateY(Plane->GZ()));
                float chargeSmall = 1000000000;
                size_t ihitSmall = 0;
                for (size_t ihit = 0; ihit < Plane->Cluster(icluster)->NHits(); ihit ++){
                    if (chargeSmall > Plane->Cluster(icluster)->Hit(ihit)->Charge()){
                        chargeSmall = Plane->Cluster(icluster)->Hit(ihit)->Charge();
                        ihitSmall = ihit;
                    }
                }
                FW->setSmallestHitCharge(iplane, chargeSmall);
                FW->setSmallestHitCharge(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->ADC());
                FW->setSmallestHitPosCol(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->Column());
                FW->setSmallestHitPosRow(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->Row());

//            if ((Plane->Cluster(icluster)->NHits() > 0)) {
//                size_t index = Plane->Cluster(icluster)->NHits() - 1;
//                if(index < FW->GetNHits()){
//                    FW->setPulseHeightsRoc(iplane,index,Plane->Cluster(icluster)->Charge());
//                }
//                else
//                    FW->setPulseHeightsRoc(iplane,FW->GetNHits()-1,Plane->Cluster(icluster)->Charge());
//            }
            }
        }
    }
    else {
        FW->setChi2(-999);
        FW->setChi2X(-999);
        FW->setChi2Y(-999);
        FW->setAngleX(-999);
        FW->setAngleY(-999);
        FW->setDia1TrackX(-999);
        FW->setDia1TrackY(-999);
        FW->setDia2TrackX(-999);
        FW->setDia2TrackY(-999);
        FW->setDistDia1(-999, -999);
        FW->setDistDia2(-999, -999);
        FW->setCoincidenceMap(0);
        for (size_t iplane = 0; iplane != FR->NPlanes(); ++iplane) {
//            PLTTrack * Track2 = FR->Track(0);
            PLTPlane * Plane = FR->Plane(iplane);
            FW->setClusters(iplane, Plane->NClusters() );
            for (size_t icluster = 0; icluster != Plane->NClusters(); icluster++) {
                FW->setChargeAll(iplane, Plane->Cluster(icluster)->Charge());
                FW->setClusterSize(iplane, Plane->Cluster(icluster)->NHits());
                FW->setClusterPositionTelescopeX(iplane, Plane->Cluster(icluster)->TX() );
                FW->setClusterPositionTelescopeY(iplane, Plane->Cluster(icluster)->TY() );
                FW->setClusterPositionLocalX(iplane, Plane->Cluster(icluster)->LX() );
                FW->setClusterPositionLocalY(iplane, Plane->Cluster(icluster)->LY() );
                FW->setResidualLocalX(iplane, -999);
                FW->setResidualLocalY(iplane, -999);
                FW->setClusterRow(iplane, Plane->Cluster(icluster)->SeedHit()->Row() );
                FW->setClusterColumn(iplane, Plane->Cluster(icluster)->SeedHit()->Column() );
                FW->setTrackX(iplane, -9999);
                FW->setTrackY(iplane, -9999);
                float chargeSmall = 1000000000;
                size_t ihitSmall = 0;
                for (size_t ihit = 0; ihit < Plane->Cluster(icluster)->NHits(); ihit ++){
                    if (chargeSmall > Plane->Cluster(icluster)->Hit(ihit)->Charge()){
                        chargeSmall = Plane->Cluster(icluster)->Hit(ihit)->Charge();
                        ihitSmall = ihit;
                    }
                }
                FW->setSmallestHitCharge(iplane, chargeSmall);
                FW->setSmallestHitCharge(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->ADC());
                FW->setSmallestHitPosCol(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->Column());
                FW->setSmallestHitPosRow(iplane, Plane->Cluster(icluster)->Hit(ihitSmall)->Row());

//            if ((Plane->Cluster(icluster)->NHits() > 0)) {
//                size_t index = Plane->Cluster(icluster)->NHits() - 1;
//                if(index < FW->GetNHits()){
//                    FW->setPulseHeightsRoc(iplane,index,Plane->Cluster(icluster)->Charge());
//                }
//                else
//                    FW->setPulseHeightsRoc(iplane,FW->GetNHits()-1,Plane->Cluster(icluster)->Charge());
//            }
            }
        }
    }
    // new Branches: DA


    FW->fillTree();
}
void PLTAnalysis::MakeAvgPH(){

    for (uint16_t i = 0; i != Histos->NRoc(); ++i) {
        for (uint16_t j = 0; j != 4; ++j) {
            Histos->AveragePH()[i][j]->Set(NGraphPoints+1);
            Histos->AveragePH()[i][j]->SetPoint(NGraphPoints, ThisTime - TimeWidth/2, Histos->dAveragePH()[i][j]);
            Histos->AveragePH()[i][j]->SetPointError(NGraphPoints, TimeWidth/2, Histos->dAveragePH()[i][j]/sqrt((float) Histos->nAveragePH()[i][j]));
            if (verbose == 1)
                printf("AvgCharge: %i %i N:%9i : %13.3E\n", i, j, Histos->nAveragePH()[i][j], Histos->dAveragePH()[i][j]);
            Histos->dAveragePH()[i][j] = 0;
            Histos->nAveragePH()[i][j] = 0;
        }
    }
    NGraphPoints++;
}
void PLTAnalysis::DrawTracks(){

    static int ieventdraw = 0;
    if (ieventdraw < 20) {
        uint16_t hp = FR->HitPlaneBits();
        if (hp == pow(2, FR->NPlanes() ) -1){
            FR->DrawTracksAndHits(TString::Format(Histos->getOutDir() + "/Tracks_Ev%i.png", ++ieventdraw).Data() );
            if (ieventdraw == 20) cout << endl;
        }
    }
}
void PLTAnalysis::FillPHHistos(uint8_t iplane, PLTCluster * Cluster){

    if (iplane < Histos->NRoc() ) {
        /** fill pulse height histo for all*/
        Histos->PulseHeight()[iplane][0]->Fill(Cluster->Charge());
        Histos->PulseHeightLong()[iplane][0]->Fill(Cluster->Charge());

        /** average pulse heights */
        PLTU::AddToRunningAverage(Histos->dAveragePH2D()[iplane][Cluster->SeedHit()->Column()][ Cluster->SeedHit()->Row()], Histos->nAveragePH2D()[iplane][Cluster->SeedHit()->Column()][ Cluster->SeedHit()->Row()], Cluster->Charge());
        PLTU::AddToRunningAverage(Histos->dAveragePH()[iplane][0], Histos->nAveragePH()[iplane][0], Cluster->Charge());

        /** fill pulse height histo one pix */
        if (Cluster->NHits() == 1) {
            Histos->PulseHeight()[iplane][1]->Fill(Cluster->Charge());
            Histos->PulseHeightLong()[iplane][1]->Fill(Cluster->Charge());
            PLTU::AddToRunningAverage(Histos->dAveragePH()[iplane][1], Histos->nAveragePH()[iplane][1], Cluster->Charge());
        }

        /** fill pulse height histo two pix */
        else if (Cluster->NHits() == 2) {
            Histos->PulseHeight()[iplane][2]->Fill(Cluster->Charge());
            Histos->PulseHeightLong()[iplane][2]->Fill(Cluster->Charge());
            PLTU::AddToRunningAverage(Histos->dAveragePH()[iplane][2], Histos->nAveragePH()[iplane][2], Cluster->Charge());
        }
        /** fill pulse height histo >3 pix */
        else if (Cluster->NHits() >= 3) {
            Histos->PulseHeight()[iplane][3]->Fill(Cluster->Charge());
            Histos->PulseHeightLong()[iplane][3]->Fill(Cluster->Charge());
            PLTU::AddToRunningAverage(Histos->dAveragePH()[iplane][3], Histos->nAveragePH()[iplane][3], Cluster->Charge());
        }
    }
}
void PLTAnalysis::FillOccupancyHiLo(PLTCluster * Cluster){

    /** fill high occupancy */
    if (Cluster->Charge() > 50000)
        for (size_t ihit = 0; ihit != Cluster->NHits(); ++ihit)
            Histos->OccupancyHighPH()[Cluster->ROC()]->Fill( Cluster->Hit(ihit)->Column(), Cluster->Hit(ihit)->Row() );

    /** fill low occupancy */
    else if (Cluster->Charge() > 10000 && Cluster->Charge() < 40000)
        for (size_t ihit = 0; ihit != Cluster->NHits(); ++ihit)
            Histos->OccupancyLowPH()[Cluster->ROC()]->Fill( Cluster->Hit(ihit)->Column(), Cluster->Hit(ihit)->Row() );
}
void PLTAnalysis::FillOccupancy(PLTPlane * Plane){

    for (size_t ihit = 0; ihit != Plane->NHits(); ++ihit) {
        PLTHit * Hit = Plane->Hit(ihit);

        if (Hit->ROC() < Histos->NRoc() ) Histos->Occupancy()[Hit->ROC()]->Fill(Hit->Column(), Hit->Row());
        else cerr << "Oops, ROC >= NROC?" << endl;
    }
}
void PLTAnalysis::FillOfflinePH(PLTTrack * Track, PLTCluster * Cluster){

    if ((fabs(Track->fAngleX) < 0.01) && (fabs(Track->fAngleY) < 0.01)){
						Histos->PulseHeightOffline()[Cluster->ROC()][0]->Fill(Cluster->Charge());

        if (Cluster->NHits() == 1)
            Histos->PulseHeightOffline()[Cluster->ROC()][1]->Fill(Cluster->Charge());
        else if (Cluster->NHits() == 2)
            Histos->PulseHeightOffline()[Cluster->ROC()][2]->Fill(Cluster->Charge());
        else if (Cluster->NHits() >= 3)
            Histos->PulseHeightOffline()[Cluster->ROC()][3]->Fill(Cluster->Charge());
    }
}
