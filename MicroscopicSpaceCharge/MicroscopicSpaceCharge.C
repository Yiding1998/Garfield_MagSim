#include <TApplication.h>
#include <TCanvas.h>
#include <TDirectory.h>
#include <TFile.h>
#include <TGraph.h>
#include <TH1D.h>
#include <TF1.h>
#include <TImage.h>
#include <TLegend.h>
#include <TMultiGraph.h>
#include <TROOT.h>
#include <TPaveText.h>
#include <TString.h>
#include <TTree.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <memory>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>
#include <iostream>

#include "Garfield/AvalancheMC.hh"
#include "Garfield/AvalancheMicroscopic.hh"
#include "Garfield/ComponentAnalyticField.hh"
#include "Garfield/ComponentChargedRing.hh"
#include "Garfield/GarfieldConstants.hh"
#include "Garfield/Medium.hh"
#include "Garfield/MediumMagboltz.hh"
#include "Garfield/Sensor.hh"
#include "Garfield/ViewDrift.hh"
#include "Garfield/ViewField.hh"

using namespace Garfield;

extern std::unique_ptr<AvalancheMicroscopic> aval;

void writeCanvasWithPngImage(TCanvas* canvas, const char* canvasName,
                             const char* pngName) {
  canvas->Modified();
  canvas->Update();
  canvas->Write(canvasName);

  TImage* image = TImage::Create();
  if (!image) return;
  image->FromPad(canvas);
  image->Write(pngName);
  delete image;
}

struct ElectronSpread {
  int nElectrons = 0;
  double meanX = 0.;
  double meanY = 0.;
  double meanZ = 0.;
  double sigmaX = 0.;
  double sigmaY = 0.;
  double sigmaZ = 0.;
  double sigmaT = 0.;
};

ElectronSpread get_electron_spread() {
  ElectronSpread spread;
  double sumX2 = 0.;
  double sumY2 = 0.;
  double sumZ2 = 0.;

  for (const auto& electron : aval->GetElectrons()) {
    const int status = electron.status;
    if (status != 0 && status != -17) continue;

    if (electron.path.empty()) continue;
    const auto& p = electron.path.back();
    ++spread.nElectrons;
    spread.meanX += p.x;
    spread.meanY += p.y;
    spread.meanZ += p.z;
    sumX2 += p.x * p.x;
    sumY2 += p.y * p.y;
    sumZ2 += p.z * p.z;
  }

  if (spread.nElectrons == 0) return spread;

  const double invN = 1. / spread.nElectrons;
  spread.meanX *= invN;
  spread.meanY *= invN;
  spread.meanZ *= invN;

  const double varX = sumX2 * invN - spread.meanX * spread.meanX;
  const double varY = sumY2 * invN - spread.meanY * spread.meanY;
  const double varZ = sumZ2 * invN - spread.meanZ * spread.meanZ;
  spread.sigmaX = std::sqrt(varX > 0. ? varX : 0.);
  spread.sigmaY = std::sqrt(varY > 0. ? varY : 0.);
  spread.sigmaZ = std::sqrt(varZ > 0. ? varZ : 0.);
  spread.sigmaT =
      std::sqrt(0.5 * (spread.sigmaX * spread.sigmaX +
                       spread.sigmaZ * spread.sigmaZ));
  return spread;
}

// These are here so they can be used within the functions defined below
std::unique_ptr<AvalancheMC> drift;
std::unique_ptr<AvalancheMicroscopic> aval;
ComponentChargedRing rings;

// set time window parameters
double tmin = 0.;
double timestep = 0.0025;
Long64_t totalIonisationElectrons = 0;
Long64_t totalAttachedElectrons = 0;
int currentEventId = -1;

struct PreviousCollisionState {
  double x = 0., y = 0., z = 0.;
  double px = 0., py = 0., pz = 0.;
};

struct CollisionHistograms {
  std::unique_ptr<TH1D> energyBefore;
  std::unique_ptr<TH1D> energyAfter;
  std::unique_ptr<TH1D> attachmentEnergyBefore;
  std::unique_ptr<TH1D> attachmentEnergyAfter;
  std::map<int, std::unique_ptr<TH1D>> energyByType;
  std::unique_ptr<TH1D> fieldImpulseMagnitude;
  std::unique_ptr<TH1D> freeFlightDeltaPMagnitude;
  std::unique_ptr<TH1D> interCollisionDeltaX;
  std::unique_ptr<TH1D> interCollisionDeltaY;
  std::unique_ptr<TH1D> interCollisionDeltaZ;
  std::unique_ptr<TH1D> interCollisionAbsDeltaX;
  std::unique_ptr<TH1D> interCollisionAbsDeltaY;
  std::unique_ptr<TH1D> interCollisionAbsDeltaZ;
  std::unique_ptr<TH1D> interCollisionDistance;
  std::unique_ptr<TH1D> positionX;
  std::unique_ptr<TH1D> positionY;
  std::unique_ptr<TH1D> positionZ;
  std::unique_ptr<TH1D> collisionTime;
};

struct SourceCounts {
  Long64_t collisions = 0;
  Long64_t ionisationElectrons = 0;
  Long64_t penningElectrons = 0;
  Long64_t attachments = 0;
};

struct ElectronBirthSource {
  int event = -1;
  int type = 0;
  int level = 0;
  int gasIndex = -1;
  std::string gasName;
  std::string process;
  int isPenning = 0;
};

std::vector<ElectronBirthSource> electronBirthSources;
std::unordered_map<std::size_t, PreviousCollisionState> previousCollisions;
std::map<std::string, SourceCounts> collisionSourceCounts;
CollisionHistograms collisionHistograms;

std::unique_ptr<TH1D> makeExtendableHistogram(const char* name,
                                              const char* title,
                                              const int bins,
                                              const double minimum,
                                              const double maximum) {
  auto histogram =
      std::make_unique<TH1D>(name, title, bins, minimum, maximum);
  histogram->SetDirectory(nullptr);
  histogram->SetCanExtend(TH1::kXaxis);
  return histogram;
}

void initialiseCollisionHistograms(const double gap) {
  collisionHistograms.energyBefore = makeExtendableHistogram(
      "collision_energy_before",
      "Energy at every real collision;energy before [eV];collisions", 300,
      0., 100.);
  collisionHistograms.energyAfter = makeExtendableHistogram(
      "collision_energy_after",
      "Energy after every real collision;energy after [eV];collisions", 300,
      0., 100.);
  collisionHistograms.attachmentEnergyBefore = makeExtendableHistogram(
      "attachment_energy_before",
      "Attached electron energy;energy before attachment [eV];electrons", 200,
      0., 100.);
  collisionHistograms.attachmentEnergyAfter = makeExtendableHistogram(
      "attachment_energy_after",
      "Post-collision attachment energy;energy after attachment [eV];electrons",
      200, 0., 100.);
  const std::array<std::pair<int, const char*>, 6> collisionTypes = {{
      {ElectronCollisionTypeElastic, "elastic"},
      {ElectronCollisionTypeIonisation, "ionisation"},
      {ElectronCollisionTypeAttachment, "attachment"},
      {ElectronCollisionTypeInelastic, "inelastic"},
      {ElectronCollisionTypeExcitation, "excitation"},
      {ElectronCollisionTypeSuperelastic, "superelastic"}}};
  for (const auto& item : collisionTypes) {
    const std::string name = std::string("collision_energy_") + item.second;
    const std::string title = std::string(item.second) +
                              " collision energy;energy before [eV];collisions";
    collisionHistograms.energyByType[item.first] = makeExtendableHistogram(
        name.c_str(), title.c_str(), 300, 0., 100.);
  }
  collisionHistograms.fieldImpulseMagnitude = makeExtendableHistogram(
      "field_impulse_magnitude",
      "Electric-field impulse between collisions;|q integral E dt| [eV/c];flights",
      300, 0., 100.);
  collisionHistograms.freeFlightDeltaPMagnitude = makeExtendableHistogram(
      "free_flight_delta_p_magnitude",
      "Total mechanical momentum change between collisions;|Delta p| [eV/c];flights",
      300, 0., 100.);
  collisionHistograms.interCollisionDeltaX = makeExtendableHistogram(
      "inter_collision_delta_x",
      "Signed displacement between collisions;#Delta x [cm];flights", 300,
      -1.e-3, 1.e-3);
  collisionHistograms.interCollisionDeltaY = makeExtendableHistogram(
      "inter_collision_delta_y",
      "Signed displacement between collisions;#Delta y [cm];flights", 300,
      -1.e-3, 1.e-3);
  collisionHistograms.interCollisionDeltaZ = makeExtendableHistogram(
      "inter_collision_delta_z",
      "Signed displacement between collisions;#Delta z [cm];flights", 300,
      -1.e-3, 1.e-3);
  collisionHistograms.interCollisionAbsDeltaX = makeExtendableHistogram(
      "inter_collision_abs_delta_x",
      "Absolute displacement between collisions;|#Delta x| [cm];flights",
      300, 0., 1.e-3);
  collisionHistograms.interCollisionAbsDeltaY = makeExtendableHistogram(
      "inter_collision_abs_delta_y",
      "Absolute displacement between collisions;|#Delta y| [cm];flights",
      300, 0., 1.e-3);
  collisionHistograms.interCollisionAbsDeltaZ = makeExtendableHistogram(
      "inter_collision_abs_delta_z",
      "Absolute displacement between collisions;|#Delta z| [cm];flights",
      300, 0., 1.e-3);
  collisionHistograms.interCollisionDistance = makeExtendableHistogram(
      "inter_collision_distance",
      "Distance between collision points;#Delta r [cm];flights", 300, 0.,
      1.e-3);
  collisionHistograms.positionX = makeExtendableHistogram(
      "collision_position_x", "Collision position;x [cm];collisions", 200,
      -0.02, 0.02);
  collisionHistograms.positionY = makeExtendableHistogram(
      "collision_position_y", "Collision position;y [cm];collisions", 200,
      0., gap);
  collisionHistograms.positionZ = makeExtendableHistogram(
      "collision_position_z", "Collision position;z [cm];collisions", 200,
      -0.02, 0.02);
  collisionHistograms.collisionTime = makeExtendableHistogram(
      "collision_time", "Collision time;t [ns];collisions", 200, 0., 5.);
}

void getCollisionSource(Medium* medium, const int level, int& gasIndex,
                        std::string& gasName, std::string& process,
                        double& thresholdEnergy) {
  gasIndex = -1;
  gasName = "unknown";
  process = "unknown";
  thresholdEnergy = 0.;
  auto* magboltz = dynamic_cast<MediumMagboltz*>(medium);
  if (!magboltz || level < 0) return;
  int sourceType = 0;
  if (!magboltz->GetLevel(static_cast<std::size_t>(level), gasIndex,
                          sourceType, process, thresholdEnergy)) {
    return;
  }
  double fraction = 0.;
  medium->GetComponent(static_cast<std::size_t>(gasIndex), gasName, fraction);
}

double momentumMagnitude(const double energy) {
  constexpr double electronRestEnergy = 510998.95;
  const double e = std::max(0., energy);
  return std::sqrt(e * (e + 2. * electronRestEnergy));
}

void userHandleCollision(double x, double y, double z, double t, int type,
                         int level, Medium* medium, double energyBefore,
                         double energyAfter, double dxBefore, double dyBefore,
                         double dzBefore, double dxAfter, double dyAfter,
                         double dzAfter, std::size_t trackId,
                         std::size_t, double fieldImpulseX,
                         double fieldImpulseY, double fieldImpulseZ) {
  int gasIndex = -1;
  std::string gasName;
  std::string process;
  double thresholdEnergy = 0.;
  getCollisionSource(medium, level, gasIndex, gasName, process,
                     thresholdEnergy);
  auto& sourceCounts = collisionSourceCounts[gasName + " | " + process];
  ++sourceCounts.collisions;

  collisionHistograms.energyBefore->Fill(energyBefore);
  collisionHistograms.energyAfter->Fill(energyAfter);
  const auto energyHistogram = collisionHistograms.energyByType.find(type);
  if (energyHistogram != collisionHistograms.energyByType.end()) {
    energyHistogram->second->Fill(energyBefore);
  }
  if (type == ElectronCollisionTypeAttachment) {
    ++sourceCounts.attachments;
    collisionHistograms.attachmentEnergyBefore->Fill(energyBefore);
    collisionHistograms.attachmentEnergyAfter->Fill(energyAfter);
  }

  const double pBefore = momentumMagnitude(energyBefore);
  const double pAfter = momentumMagnitude(energyAfter);
  const double momentumBeforeX = pBefore * dxBefore;
  const double momentumBeforeY = pBefore * dyBefore;
  const double momentumBeforeZ = pBefore * dzBefore;
  const double momentumAfterX = pAfter * dxAfter;
  const double momentumAfterY = pAfter * dyAfter;
  const double momentumAfterZ = pAfter * dzAfter;
  const double fieldImpulseMagnitude =
      std::sqrt(fieldImpulseX * fieldImpulseX +
                fieldImpulseY * fieldImpulseY +
                fieldImpulseZ * fieldImpulseZ);
  collisionHistograms.fieldImpulseMagnitude->Fill(fieldImpulseMagnitude);
  collisionHistograms.positionX->Fill(x);
  collisionHistograms.positionY->Fill(y);
  collisionHistograms.positionZ->Fill(z);
  collisionHistograms.collisionTime->Fill(t);

  const auto previous = previousCollisions.find(trackId);
  if (previous != previousCollisions.end()) {
    const double deltaPX = momentumBeforeX - previous->second.px;
    const double deltaPY = momentumBeforeY - previous->second.py;
    const double deltaPZ = momentumBeforeZ - previous->second.pz;
    collisionHistograms.freeFlightDeltaPMagnitude->Fill(
        std::sqrt(deltaPX * deltaPX + deltaPY * deltaPY + deltaPZ * deltaPZ));

    const double deltaX = x - previous->second.x;
    const double deltaY = y - previous->second.y;
    const double deltaZ = z - previous->second.z;
    collisionHistograms.interCollisionDeltaX->Fill(deltaX);
    collisionHistograms.interCollisionDeltaY->Fill(deltaY);
    collisionHistograms.interCollisionDeltaZ->Fill(deltaZ);
    collisionHistograms.interCollisionAbsDeltaX->Fill(std::abs(deltaX));
    collisionHistograms.interCollisionAbsDeltaY->Fill(std::abs(deltaY));
    collisionHistograms.interCollisionAbsDeltaZ->Fill(std::abs(deltaZ));
    collisionHistograms.interCollisionDistance->Fill(
        std::sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ));
  }
  previousCollisions[trackId] = {x, y, z, momentumAfterX, momentumAfterY,
                                 momentumAfterZ};
}

void userHandleIonisation(double x, double y, double z, double t, int type,
                          int level, Medium* m) {
  ++totalIonisationElectrons;
  ElectronBirthSource source;
  source.event = currentEventId;
  source.type = type;
  source.level = level;
  double thresholdEnergy = 0.;
  getCollisionSource(m, level, source.gasIndex, source.gasName,
                     source.process, thresholdEnergy);
  source.isPenning = type == ElectronCollisionTypeExcitation;
  auto& sourceCounts =
      collisionSourceCounts[source.gasName + " | " + source.process];
  ++sourceCounts.ionisationElectrons;
  if (source.isPenning) ++sourceCounts.penningElectrons;
  electronBirthSources.push_back(std::move(source));
  drift->AddIon(x, y, z,
               tmin + timestep);  // ion added at the start of the next timestep
}

void userHandleAttachment(double x, double y, double z, double t, int type,
                          int level, Medium* m) {
  ++totalAttachedElectrons;
  drift->AddNegativeIon(
      x, y, z, tmin + timestep);  // ion added at the start of the next timestep
}

void get_mean(std::array<double, 3>& mean_pos,
              std::array<int, 3>& particle_counts, int& n_particles,
              bool enableSpaceCharge) {
  // Finds the mean of the particle positions and number
  // of particles in the avalanche. Also add the charged
  // rings if space-charge is enabled.
  //
  // particle counts is {electrons, ions, negative ions}

  mean_pos = {0., 0., 0.};
  particle_counts = {
      0,
      0,
      0,
  };

  for (const auto& electron : aval->GetElectrons()) {
    // get electron positions
    if (electron.path.empty()) continue;
    double xf = electron.path.back().x;
    double yf = electron.path.back().y;
    double zf = electron.path.back().z;
    int status = electron.status;
    // if at the end of timestep and within the region of interest:
    // status == 0 allows for the first electron to be added
    if (status == 0 || status == -17) {
      particle_counts[0]++;
      mean_pos[0] += xf;
      mean_pos[1] += yf;
      mean_pos[2] += zf;
    }
  }
  for (const auto& ion : drift->GetIons()) {
    // get positive ion positions
    if (ion.path.empty()) continue;
    double xf = ion.path.back().x;
    double yf = ion.path.back().y;
    double zf = ion.path.back().z;
    int status = ion.status;
    // if at the end of timestep and within the region of interest:
    if (status == 0 || status == -17) {
      particle_counts[1]++;
      mean_pos[0] += xf;
      mean_pos[1] += yf;
      mean_pos[2] += zf;
    }
  }
  for (const auto& ion : drift->GetNegativeIons()) {
    // get negative ion positions
    if (ion.path.empty()) continue;
    double xf = ion.path.back().x;
    double yf = ion.path.back().y;
    double zf = ion.path.back().z;
    int status = ion.status;
    // if at the end of timestep and within the region of interest:
    if (status == 0 || status == -17) {
      particle_counts[2]++;
      mean_pos[0] += xf;
      mean_pos[1] += yf;
      mean_pos[2] += zf;
    }
  }

  n_particles = particle_counts[0] + particle_counts[1] + particle_counts[2];
  if (n_particles == 0) return;
  for (int j = 0; j < 3; ++j) {
    mean_pos[j] /= static_cast<double>(n_particles);
  }
}


struct SpaceChargeBin {
  Long64_t count = 0;
  double charge = 0.;
  double sumX = 0.;
  double sumY = 0.;
  double sumZ = 0.;
};

void addToSpaceChargeBins(std::vector<SpaceChargeBin>& bins, const double x,
                          const double y, const double z,
                          const double charge, const double yMin,
                          const double binWidth) {
  if (bins.empty()) return;
  int bin = static_cast<int>((y - yMin) / binWidth);
  bin = std::clamp(bin, 0, static_cast<int>(bins.size()) - 1);
  auto& target = bins[bin];
  ++target.count;
  target.charge += charge;
  target.sumX += x;
  target.sumY += y;
  target.sumZ += z;
}

void addBinnedSpaceChargeRings(const std::vector<SpaceChargeBin>& bins) {
  for (const auto& bin : bins) {
    if (bin.count == 0 || bin.charge == 0.) continue;
    const double invCount = 1. / static_cast<double>(bin.count);
    rings.AddChargedRing(bin.sumX * invCount, bin.sumY * invCount,
                         bin.sumZ * invCount, bin.charge);
  }
}

void buildBinnedSpaceCharge(const double yMin, const double yMax,
                            const double binWidth) {
  const double gap = yMax - yMin;
  const int nBins =
      std::max(1, static_cast<int>(std::ceil(gap / binWidth)));
  std::vector<SpaceChargeBin> electronBins(nBins);
  std::vector<SpaceChargeBin> ionBins(nBins);
  std::vector<SpaceChargeBin> negativeIonBins(nBins);

  for (const auto& electron : aval->GetElectrons()) {
    if (electron.path.empty()) continue;
    const int status = electron.status;
    if (status != StatusAlive && status != StatusOutsideTimeWindow) continue;
    const auto& p = electron.path.back();
    addToSpaceChargeBins(electronBins, p.x, p.y, p.z, -1., yMin, binWidth);
  }
  for (const auto& ion : drift->GetIons()) {
    if (ion.path.empty()) continue;
    const int status = ion.status;
    if (status != StatusAlive && status != StatusOutsideTimeWindow) continue;
    const auto& p = ion.path.back();
    addToSpaceChargeBins(ionBins, p.x, p.y, p.z, 1., yMin, binWidth);
  }
  for (const auto& ion : drift->GetNegativeIons()) {
    if (ion.path.empty()) continue;
    const int status = ion.status;
    if (status != StatusAlive && status != StatusOutsideTimeWindow) continue;
    const auto& p = ion.path.back();
    addToSpaceChargeBins(negativeIonBins, p.x, p.y, p.z, -1., yMin, binWidth);
  }

  addBinnedSpaceChargeRings(electronBins);
  addBinnedSpaceChargeRings(ionBins);
  addBinnedSpaceChargeRings(negativeIonBins);
}

void writeCollisionStatistics() {
  collisionHistograms.energyBefore->Write();
  collisionHistograms.energyAfter->Write();
  collisionHistograms.attachmentEnergyBefore->Write();
  collisionHistograms.attachmentEnergyAfter->Write();
  collisionHistograms.fieldImpulseMagnitude->Write();
  collisionHistograms.freeFlightDeltaPMagnitude->Write();
  collisionHistograms.interCollisionDeltaX->Write();
  collisionHistograms.interCollisionDeltaY->Write();
  collisionHistograms.interCollisionDeltaZ->Write();
  collisionHistograms.interCollisionAbsDeltaX->Write();
  collisionHistograms.interCollisionAbsDeltaY->Write();
  collisionHistograms.interCollisionAbsDeltaZ->Write();
  collisionHistograms.interCollisionDistance->Write();
  collisionHistograms.positionX->Write();
  collisionHistograms.positionY->Write();
  collisionHistograms.positionZ->Write();
  collisionHistograms.collisionTime->Write();
  for (auto& item : collisionHistograms.energyByType) item.second->Write();

  TTree birthTree("electron_birth_sources",
                  "Molecular source of avalanche-produced electrons");
  ElectronBirthSource birth;
  birthTree.Branch("event", &birth.event);
  birthTree.Branch("type", &birth.type);
  birthTree.Branch("level", &birth.level);
  birthTree.Branch("gasIndex", &birth.gasIndex);
  birthTree.Branch("gasName", &birth.gasName);
  birthTree.Branch("process", &birth.process);
  birthTree.Branch("isPenning", &birth.isPenning);
  for (const auto& source : electronBirthSources) {
    birth = source;
    birthTree.Fill();
  }
  birthTree.Write();

  TTree sourceTree("collision_source_summary",
                   "Collision, ionisation and attachment counts by source");
  std::string sourceLabel;
  Long64_t sourceCollisions = 0, sourceIonisations = 0;
  Long64_t sourcePenning = 0, sourceAttachments = 0;
  sourceTree.Branch("source", &sourceLabel);
  sourceTree.Branch("collisions", &sourceCollisions);
  sourceTree.Branch("ionisationElectrons", &sourceIonisations);
  sourceTree.Branch("penningElectrons", &sourcePenning);
  sourceTree.Branch("attachments", &sourceAttachments);
  const int nSources =
      std::max(1, static_cast<int>(collisionSourceCounts.size()));
  TH1D collisionSources("collision_sources_all",
                        "Real collision molecular sources;source;collisions",
                        nSources, 0., nSources);
  TH1D ionisationSources("ionisation_electron_sources",
                         "Avalanche-electron molecular sources;source;electrons",
                         nSources, 0., nSources);
  TH1D attachmentSources("attachment_sources",
                         "Electron attachment molecular sources;source;electrons",
                         nSources, 0., nSources);
  int sourceBin = 1;
  for (const auto& item : collisionSourceCounts) {
    sourceLabel = item.first;
    sourceCollisions = item.second.collisions;
    sourceIonisations = item.second.ionisationElectrons;
    sourcePenning = item.second.penningElectrons;
    sourceAttachments = item.second.attachments;
    sourceTree.Fill();
    collisionSources.GetXaxis()->SetBinLabel(sourceBin, sourceLabel.c_str());
    ionisationSources.GetXaxis()->SetBinLabel(sourceBin, sourceLabel.c_str());
    attachmentSources.GetXaxis()->SetBinLabel(sourceBin, sourceLabel.c_str());
    collisionSources.SetBinContent(sourceBin, sourceCollisions);
    ionisationSources.SetBinContent(sourceBin, sourceIonisations);
    attachmentSources.SetBinContent(sourceBin, sourceAttachments);
    ++sourceBin;
  }
  sourceTree.Write();
  collisionSources.Write();
  ionisationSources.Write();
  attachmentSources.Write();

  TCanvas* collisionCanvas =
      new TCanvas("collision_statistics", "", 1200, 900);
  collisionCanvas->Divide(3, 2);
  collisionCanvas->cd(1);
  collisionHistograms.energyBefore->Draw();
  collisionCanvas->cd(2);
  collisionHistograms.energyAfter->Draw();
  collisionCanvas->cd(3);
  collisionHistograms.attachmentEnergyBefore->Draw();
  collisionCanvas->cd(4);
  collisionHistograms.attachmentEnergyAfter->Draw();
  collisionCanvas->cd(5);
  collisionHistograms.fieldImpulseMagnitude->Draw();
  collisionCanvas->cd(6);
  collisionHistograms.freeFlightDeltaPMagnitude->Draw();
  writeCanvasWithPngImage(collisionCanvas, "collision_statistics",
                          "collision_statistics_png");

  TCanvas* typeCanvas =
      new TCanvas("collision_energy_by_type", "", 1200, 800);
  typeCanvas->Divide(3, 2);
  int typePad = 1;
  for (auto& item : collisionHistograms.energyByType) {
    typeCanvas->cd(typePad++);
    item.second->Draw();
  }
  writeCanvasWithPngImage(typeCanvas, "collision_energy_by_type",
                          "collision_energy_by_type_png");

  TCanvas* signedDisplacementCanvas =
      new TCanvas("inter_collision_signed_displacement", "", 1200, 400);
  signedDisplacementCanvas->Divide(3, 1);
  signedDisplacementCanvas->cd(1);
  collisionHistograms.interCollisionDeltaX->Draw();
  signedDisplacementCanvas->cd(2);
  collisionHistograms.interCollisionDeltaY->Draw();
  signedDisplacementCanvas->cd(3);
  collisionHistograms.interCollisionDeltaZ->Draw();
  writeCanvasWithPngImage(signedDisplacementCanvas,
                          "inter_collision_signed_displacement",
                          "inter_collision_signed_displacement_png");

  TCanvas* displacementCanvas =
      new TCanvas("inter_collision_distance_statistics", "", 1100, 800);
  displacementCanvas->Divide(2, 2);
  displacementCanvas->cd(1);
  collisionHistograms.interCollisionAbsDeltaX->Draw();
  displacementCanvas->cd(2);
  collisionHistograms.interCollisionAbsDeltaY->Draw();
  displacementCanvas->cd(3);
  collisionHistograms.interCollisionAbsDeltaZ->Draw();
  displacementCanvas->cd(4);
  collisionHistograms.interCollisionDistance->Draw();
  writeCanvasWithPngImage(displacementCanvas,
                          "inter_collision_distance_statistics",
                          "inter_collision_distance_png");

  TCanvas* positionCanvas =
      new TCanvas("collision_position_time_statistics", "", 1100, 800);
  positionCanvas->Divide(2, 2);
  positionCanvas->cd(1);
  collisionHistograms.positionX->Draw();
  positionCanvas->cd(2);
  collisionHistograms.positionY->Draw();
  positionCanvas->cd(3);
  collisionHistograms.positionZ->Draw();
  positionCanvas->cd(4);
  collisionHistograms.collisionTime->Draw();
  writeCanvasWithPngImage(positionCanvas,
                          "collision_position_time_statistics",
                          "collision_position_time_statistics_png");

  TCanvas* sourceCanvas = new TCanvas("collision_sources", "", 1500, 600);
  sourceCanvas->Divide(3, 1);
  sourceCanvas->cd(1);
  collisionSources.LabelsOption("v", "X");
  collisionSources.Draw("hist");
  sourceCanvas->cd(2);
  ionisationSources.LabelsOption("v", "X");
  ionisationSources.Draw("hist");
  sourceCanvas->cd(3);
  attachmentSources.LabelsOption("v", "X");
  attachmentSources.Draw("hist");
  writeCanvasWithPngImage(sourceCanvas, "collision_sources",
                          "collision_sources_png");
}

int main(int argc, char* argv[]) {
  const int inputArgc = argc;
  std::vector<TString> inputArgs;
  inputArgs.reserve(inputArgc);
  for (int i = 0; i < inputArgc; ++i) inputArgs.emplace_back(argv[i]);

  int nEvents = 3;
  int gapUm = 215;
  double pressureAtm = 1.;
  int voltage = -750;
  double magFieldY = 0.;
  bool enableSpaceCharge = true;

  if (inputArgc > 1) nEvents = std::atoi(inputArgs[1].Data());
  if (inputArgc > 2) gapUm = std::atoi(inputArgs[2].Data());
  if (inputArgc > 3) pressureAtm = std::atof(inputArgs[3].Data());
  if (inputArgc > 4) voltage = -std::abs(std::atoi(inputArgs[4].Data()));
  if (inputArgc > 5) magFieldY = std::atof(inputArgs[5].Data());
  if (inputArgc > 6) enableSpaceCharge = std::atoi(inputArgs[6].Data()) != 0;

  gROOT->SetBatch(kTRUE);
  TApplication app("app", &argc, argv);

  const double posBottomPlane = 0.;
  const double posTopPlane = gapUm * 1.e-4;
  initialiseCollisionHistograms(posTopPlane - posBottomPlane);

  const bool enableDebug = false;
  const bool plotField = true;
  const bool plotDrift = false;
  const bool enableAdaptiveTimestep = true;
  constexpr std::size_t avalancheSizeLimit = 50000; // 5000
  constexpr double spaceChargeBinWidth = 2.e-4;
  constexpr Long64_t maxTotalIonisations = 500000;  // 500000

  const double x0 = 0.;
  const double y0 = posTopPlane - 0.1e-4;
  const double z0 = 0.;
  const double t0 = 0.;
  const double e0 = 0.1;

  const TString rootFileName = TString::Format(
          "%dum_%.2fatm_%dV_%.2fT_%de_SC%d.root", gapUm, pressureAtm,
          std::abs(voltage), magFieldY, nEvents, enableSpaceCharge ? 1 : 0);
      std::cout << "\nWriting output to " << rootFileName << "\n";

      MediumMagboltz gas;
      // gas.SetComposition("ar", 93, "co2", 7);
      gas.SetComposition("C2H2F4", 60., "iC4H10", 30., "SF6", 10.);  // [%]
      // gas.SetComposition("ar", 80, "co2", 10, "iC4H10", 10);  // [%]
      gas.SetTemperature(293.15);
      gas.SetPressure(760. * pressureAtm);
      gas.LoadIonMobility("IonMobility_Ar+_Ar.txt");
      gas.SetMaxElectronEnergy(5000.);
      gas.Initialise(true);
      gas.EnablePenningTransfer();

      ComponentAnalyticField pp;
      pp.SetMedium(&gas);
      pp.SetMagneticField(0., magFieldY, 0.);
      pp.AddPlaneY(posBottomPlane, 0.);
      pp.AddPlaneY(posTopPlane, voltage);

      rings.ClearActiveRings();
      rings.SetArea(-0.02, posBottomPlane, -0.02, 0.02, posTopPlane, 0.02);
      rings.SetMedium(&gas);
      rings.SetMagneticField(0., magFieldY, 0.);
      rings.SetSpacingTolerance(0.00005);
      rings.SetSelfFieldTolerance(0.00001);

      Sensor sensor;
      sensor.AddComponent(&pp);
      if (enableSpaceCharge) sensor.AddComponent(&rings);

      ViewField fieldView;
      if (plotField) {
        fieldView.SetSensor(&sensor);
        fieldView.SetPlane(0., 0., 1., 0., 0., 0.);
        fieldView.SetArea(-0.01, posBottomPlane, 0.01, posTopPlane);
      }

      ViewDrift driftView;
      if (plotDrift) {
        driftView.SetCollisionMarkerSize(0.0000001);
        driftView.SetColourIonisations(7);
        driftView.SetPlane(0, 0, 1, 0, 0, 0);
        driftView.SetArea(-0.01, posBottomPlane, 0.01, posTopPlane);
      }

      std::vector<int> eventIds;
      std::vector<int> eventGains;
      std::vector<int> eventAvalancheIons;
      std::vector<Long64_t> eventIonisations;
      std::vector<Long64_t> eventAttachments;
      std::vector<Long64_t> eventCollectedElectrons;
      std::vector<int> eventStoppedByIonisationLimit;

      std::vector<int> endpointEvents;
      std::vector<int> endpointStatuses;
      std::vector<Long64_t> endpointWeights;
      std::vector<double> endpointX;
      std::vector<double> endpointY;
      std::vector<double> endpointZ;
      std::vector<double> endpointTime;
      std::vector<double> collectedEndpointX;
      std::vector<double> collectedEndpointY;
      std::vector<double> collectedEndpointZ;
      std::vector<double> collectedEndpointTime;
      std::vector<Long64_t> collectedEndpointWeights;

      std::vector<int> diffusionEvents;
      std::vector<int> diffusionFrames;
      std::vector<int> diffusionElectronCounts;
      std::vector<double> diffusionTimes;
      std::vector<double> diffusionMeanX;
      std::vector<double> diffusionMeanY;
      std::vector<double> diffusionMeanZ;
      std::vector<double> diffusionSigmaX;
      std::vector<double> diffusionSigmaY;
      std::vector<double> diffusionSigmaZ;
      std::vector<double> diffusionSigmaT;

      for (int event = 0; event < nEvents; ++event) {
        currentEventId = event;
        previousCollisions.clear();
        std::cout << "\n========== Avalanche event " << event
                  << " (p=" << pressureAtm << " atm, By=" << magFieldY
                  << " T) ==========\n";

        tmin = 0.;
        timestep = 0.0025;
        totalIonisationElectrons = 0;
        totalAttachedElectrons = 0;
        rings.ClearActiveRings();

        aval = std::make_unique<AvalancheMicroscopic>();
        drift = std::make_unique<AvalancheMC>();
        aval->SetSensor(&sensor);
        aval->EnableMagneticField(magFieldY != 0.);
        aval->EnableAvalancheSizeLimit(avalancheSizeLimit);
        drift->SetSensor(&sensor);
        aval->SetUserHandleIonisation(userHandleIonisation);
        aval->SetUserHandleAttachment(userHandleAttachment);
        aval->SetUserHandleCollision(userHandleCollision);

        if (plotDrift) {
          aval->EnablePlotting(&driftView);
          drift->EnablePlotting(&driftView);
        }
        if (enableDebug) {
          rings.EnableDebugging();
          aval->EnableDebugging();
          drift->EnableDebugging();
        }

        aval->AddElectron(x0, y0, z0, t0, e0);

        int currentIonCount = 0;
        int currentElectronCount = 0;
        int frameNumber = -1;
        int nParticles = 0;
        std::array<double, 3> meanPosition;
        std::array<int, 3> particleCounts;

        auto recordDiffusion = [&](const int frame, const double time) {
          const ElectronSpread spread = get_electron_spread();
          diffusionEvents.push_back(event);
          diffusionFrames.push_back(frame);
          diffusionTimes.push_back(time);
          diffusionElectronCounts.push_back(spread.nElectrons);
          diffusionMeanX.push_back(spread.meanX);
          diffusionMeanY.push_back(spread.meanY);
          diffusionMeanZ.push_back(spread.meanZ);
          diffusionSigmaX.push_back(spread.sigmaX);
          diffusionSigmaY.push_back(spread.sigmaY);
          diffusionSigmaZ.push_back(spread.sigmaZ);
          diffusionSigmaT.push_back(spread.sigmaT);
        };

        Long64_t collectedElectrons = 0;
        int stoppedByIonisationLimit = 0;
        constexpr double collectionTolerance = 5.e-5;
        auto collectTerminatedEndpoints = [&]() {
          for (const auto& electron : aval->GetElectrons()) {
            if (electron.path.empty() || electron.status == StatusAlive ||
                electron.status == StatusOutsideTimeWindow) {
              continue;
            }
            const auto& endpoint = electron.path.back();
            const Long64_t weight = static_cast<Long64_t>(electron.weight);
            endpointEvents.push_back(event);
            endpointStatuses.push_back(electron.status);
            endpointWeights.push_back(weight);
            endpointX.push_back(endpoint.x);
            endpointY.push_back(endpoint.y);
            endpointZ.push_back(endpoint.z);
            endpointTime.push_back(endpoint.t);

            const bool reachedBottomPlane =
                (electron.status == StatusLeftDriftMedium ||
                 electron.status == StatusHitPlane) &&
                std::abs(endpoint.y - posBottomPlane) <= collectionTolerance;
            if (!reachedBottomPlane) continue;

            collectedEndpointX.push_back(endpoint.x);
            collectedEndpointY.push_back(endpoint.y);
            collectedEndpointZ.push_back(endpoint.z);
            collectedEndpointTime.push_back(endpoint.t);
            collectedEndpointWeights.push_back(weight);
            collectedElectrons += weight;
          }
        };

        while (true) {
          ++frameNumber;
          nParticles = 0;
          if (enableSpaceCharge) rings.ClearActiveRings();
          get_mean(meanPosition, particleCounts, nParticles, enableSpaceCharge);
          if (enableSpaceCharge) {
            buildBinnedSpaceCharge(posBottomPlane, posTopPlane,
                                   spaceChargeBinWidth);
          }

          std::cout << "Event " << event << ", frame " << frameNumber
                    << ", timestep " << timestep << " ns: e-="
                    << particleCounts[0] << ", i+=" << particleCounts[1]
                    << ", i-=" << particleCounts[2] << "\n";

          if (enableAdaptiveTimestep && currentIonCount > 0 &&
              currentElectronCount > 0) {
            const int newIons = particleCounts[1] - currentIonCount;
            const double ratio =
                newIons / static_cast<double>(currentElectronCount);
            constexpr double tolerance = 0.2;
            if (newIons < 10 && particleCounts[0] < 100) {
              timestep = 0.05;
            } else if (ratio < tolerance) {
              timestep /= 1. - ratio;
            } else if (ratio > tolerance) {
              timestep /= 1. + ratio;
            }
          } else {
            timestep = 0.05;
          }
          currentIonCount = particleCounts[1];
          currentElectronCount = particleCounts[0];

          if (nParticles > 0) {
            rings.UpdateCentre(meanPosition[0], meanPosition[2]);
          }

          if (totalIonisationElectrons >= maxTotalIonisations) {
            stoppedByIonisationLimit = 1;
            std::cout << "Reached max total ionisations ("
                      << maxTotalIonisations << "), stopping event.\n";
            recordDiffusion(frameNumber, tmin);
            break;
          }

          if (particleCounts[0] == 0) {
            recordDiffusion(frameNumber, tmin);
            break;
          }

          aval->SetTimeWindow(tmin, tmin + timestep);
          aval->ResumeAvalanche();
          collectTerminatedEndpoints();
          if (particleCounts[1] > 0 || particleCounts[2] > 0) {
            drift->SetTimeWindow(tmin, tmin + timestep);
            drift->ResumeAvalanche();
          }
          tmin += timestep;
          recordDiffusion(frameNumber, tmin);
        }

        const int avalancheElectrons =
            static_cast<int>(1 + totalIonisationElectrons);
        const int avalancheIons = static_cast<int>(totalIonisationElectrons);

        eventIds.push_back(event);
        eventGains.push_back(avalancheElectrons);
        eventAvalancheIons.push_back(avalancheIons);
        eventIonisations.push_back(totalIonisationElectrons);
        eventAttachments.push_back(totalAttachedElectrons);
        eventCollectedElectrons.push_back(collectedElectrons);
        eventStoppedByIonisationLimit.push_back(stoppedByIonisationLimit);

        std::cout << "Event " << event << " gain: " << avalancheElectrons
                  << ", ionisations: " << totalIonisationElectrons
                  << ", attachments: " << totalAttachedElectrons
                  << ", collected electrons: " << collectedElectrons << "\n";
      }

      TFile outputFile(rootFileName, "RECREATE");
      writeCollisionStatistics();
      Long64_t allIonisations = 0;
      Long64_t allAttachments = 0;
      Long64_t allCollectedElectrons = 0;
      for (std::size_t i = 0; i < eventIds.size(); ++i) {
        allIonisations += eventIonisations[i];
        allAttachments += eventAttachments[i];
        allCollectedElectrons += eventCollectedElectrons[i];
      }
      TTree runSummary("run_summary", "Run configuration and total statistics");
      int summaryNumberOfEvents = nEvents;
      Long64_t summaryAvalancheSizeLimit =
          static_cast<Long64_t>(avalancheSizeLimit);
      Long64_t summaryMaxTotalIonisations = maxTotalIonisations;
      double summarySpaceChargeBinWidth = spaceChargeBinWidth;
      int summarySpaceChargeNBins =
          std::max(1, static_cast<int>(std::ceil((posTopPlane - posBottomPlane) /
                                                spaceChargeBinWidth)));
      Long64_t summaryTotalIonisations = allIonisations;
      Long64_t summaryTotalAttachments = allAttachments;
      Long64_t summaryTotalCollectedElectrons = allCollectedElectrons;
      double summaryGapThickness = posTopPlane - posBottomPlane;
      int summaryGapUm = gapUm;
      double summaryPressureAtm = pressureAtm;
      double summaryPressureTorr = 760. * pressureAtm;
      double summaryVoltage = voltage;
      double summaryMagFieldY = magFieldY;
      int summaryEnableSpaceCharge = enableSpaceCharge ? 1 : 0;
      double summaryInitialX = x0;
      double summaryInitialY = y0;
      double summaryInitialZ = z0;
      double summaryInitialEnergy = e0;
      runSummary.Branch("numberOfEvents", &summaryNumberOfEvents);
      runSummary.Branch("avalancheSizeLimit", &summaryAvalancheSizeLimit);
      runSummary.Branch("maxTotalIonisations", &summaryMaxTotalIonisations);
      runSummary.Branch("spaceChargeBinWidth", &summarySpaceChargeBinWidth);
      runSummary.Branch("spaceChargeNBins", &summarySpaceChargeNBins);
      runSummary.Branch("totalIonisations", &summaryTotalIonisations);
      runSummary.Branch("totalAttachments", &summaryTotalAttachments);
      runSummary.Branch("totalCollectedElectrons",
                        &summaryTotalCollectedElectrons);
      runSummary.Branch("gapThickness", &summaryGapThickness);
      runSummary.Branch("gapUm", &summaryGapUm);
      runSummary.Branch("pressureAtm", &summaryPressureAtm);
      runSummary.Branch("pressureTorr", &summaryPressureTorr);
      runSummary.Branch("voltage", &summaryVoltage);
      runSummary.Branch("magFieldY", &summaryMagFieldY);
      runSummary.Branch("enableSpaceCharge", &summaryEnableSpaceCharge);
      runSummary.Branch("initialX", &summaryInitialX);
      runSummary.Branch("initialY", &summaryInitialY);
      runSummary.Branch("initialZ", &summaryInitialZ);
      runSummary.Branch("initialEnergy", &summaryInitialEnergy);
      runSummary.Fill();
      runSummary.Write();

      TTree eventTree("avalanche_events", "Per-event avalanche statistics");
      int eventId = 0;
      int eventGain = 0;
      int eventIonCount = 0;
      int eventStoppedByLimit = 0;
      Long64_t eventIonisationCount = 0;
      Long64_t eventAttachmentCount = 0;
      Long64_t eventCollectedCount = 0;
      eventTree.Branch("event", &eventId);
      eventTree.Branch("gain", &eventGain);
      eventTree.Branch("avalancheIons", &eventIonCount);
      eventTree.Branch("ionisations", &eventIonisationCount);
      eventTree.Branch("attachments", &eventAttachmentCount);
      eventTree.Branch("collectedElectrons", &eventCollectedCount);
      eventTree.Branch("stoppedByIonisationLimit", &eventStoppedByLimit);
      for (std::size_t i = 0; i < eventIds.size(); ++i) {
        eventId = eventIds[i];
        eventGain = eventGains[i];
        eventIonCount = eventAvalancheIons[i];
        eventIonisationCount = eventIonisations[i];
        eventAttachmentCount = eventAttachments[i];
        eventCollectedCount = eventCollectedElectrons[i];
        eventStoppedByLimit = eventStoppedByIonisationLimit[i];
        eventTree.Fill();
      }
      eventTree.Write();

      TTree endpointTree("electron_endpoints", "Terminal electron endpoints");
      int endpointEvent = 0;
      int endpointStatus = 0;
      Long64_t endpointWeight = 0;
      double endpointXValue = 0.;
      double endpointYValue = 0.;
      double endpointZValue = 0.;
      double endpointTimeValue = 0.;
      endpointTree.Branch("event", &endpointEvent);
      endpointTree.Branch("status", &endpointStatus);
      endpointTree.Branch("weight", &endpointWeight);
      endpointTree.Branch("x", &endpointXValue);
      endpointTree.Branch("y", &endpointYValue);
      endpointTree.Branch("z", &endpointZValue);
      endpointTree.Branch("time", &endpointTimeValue);
      for (std::size_t i = 0; i < endpointX.size(); ++i) {
        endpointEvent = endpointEvents[i];
        endpointStatus = endpointStatuses[i];
        endpointWeight = endpointWeights[i];
        endpointXValue = endpointX[i];
        endpointYValue = endpointY[i];
        endpointZValue = endpointZ[i];
        endpointTimeValue = endpointTime[i];
        endpointTree.Fill();
      }
      endpointTree.Write();

      auto makeRange = [](const std::vector<double>& values,
                          const double defaultMin, const double defaultMax) {
        if (values.empty()) return std::make_pair(defaultMin, defaultMax);
        const auto limits = std::minmax_element(values.begin(), values.end());
        double minimum = *limits.first;
        double maximum = *limits.second;
        double padding = 0.1 * (maximum - minimum);
        if (padding <= 0.) padding = std::max(1.e-8, 0.1 * std::abs(minimum));
        return std::make_pair(minimum - padding, maximum + padding);
      };

      const auto rangeX = makeRange(collectedEndpointX, -0.01, 0.01);
      const auto rangeY = makeRange(collectedEndpointY, -1.e-5, 1.e-5);
      const auto rangeZ = makeRange(collectedEndpointZ, -0.01, 0.01);
      const auto rangeTime = makeRange(collectedEndpointTime, 0., 2.);
      TH1D endpointXHist("endpoint_x",
                         "Collected electron endpoints;x [cm];electrons", 100,
                         rangeX.first, rangeX.second);
      TH1D endpointYHist("endpoint_y",
                         "Collected electron endpoints;y [cm];electrons", 100,
                         rangeY.first, rangeY.second);
      TH1D endpointZHist("endpoint_z",
                         "Collected electron endpoints;z [cm];electrons", 100,
                         rangeZ.first, rangeZ.second);
      TH1D endpointTimeHist(
          "endpoint_time", "Collected electron arrival time;t [ns];electrons",
          100, rangeTime.first, rangeTime.second);
      for (std::size_t i = 0; i < collectedEndpointX.size(); ++i) {
        const double weight = static_cast<double>(collectedEndpointWeights[i]);
        endpointXHist.Fill(collectedEndpointX[i], weight);
        endpointYHist.Fill(collectedEndpointY[i], weight);
        endpointZHist.Fill(collectedEndpointZ[i], weight);
        endpointTimeHist.Fill(collectedEndpointTime[i], weight);
      }

      auto fitGaussian = [](TH1D& histogram, const char* name, TF1& function,
                            double& fitMin, double& fitMax) {
        function.SetName(name);
        if (histogram.GetEffectiveEntries() < 10 || histogram.GetRMS() <= 0.) {
          fitMin = histogram.GetXaxis()->GetXmin();
          fitMax = histogram.GetXaxis()->GetXmax();
          return -1;
        }
        const double mean = histogram.GetMean();
        const double sigma = histogram.GetRMS();
        fitMin = std::max(histogram.GetXaxis()->GetXmin(), mean - 3. * sigma);
        fitMax = std::min(histogram.GetXaxis()->GetXmax(), mean + 3. * sigma);
        function.SetRange(fitMin, fitMax);
        return static_cast<int>(histogram.Fit(&function, "Q0R"));
      };

      TF1 fitEndpointX("fit_endpoint_x", "gaus", rangeX.first, rangeX.second);
      TF1 fitEndpointY("fit_endpoint_y", "gaus", rangeY.first, rangeY.second);
      TF1 fitEndpointZ("fit_endpoint_z", "gaus", rangeZ.first, rangeZ.second);
      TF1 fitEndpointTime("fit_endpoint_time", "gaus", rangeTime.first,
                          rangeTime.second);
      double fitMinX = 0.;
      double fitMaxX = 0.;
      double fitMinY = 0.;
      double fitMaxY = 0.;
      double fitMinZ = 0.;
      double fitMaxZ = 0.;
      double fitMinTime = 0.;
      double fitMaxTime = 0.;
      const int fitStatusX =
          fitGaussian(endpointXHist, "fit_endpoint_x", fitEndpointX, fitMinX,
                      fitMaxX);
      const int fitStatusY =
          fitGaussian(endpointYHist, "fit_endpoint_y", fitEndpointY, fitMinY,
                      fitMaxY);
      const int fitStatusZ =
          fitGaussian(endpointZHist, "fit_endpoint_z", fitEndpointZ, fitMinZ,
                      fitMaxZ);
      const int fitStatusTime = fitGaussian(endpointTimeHist,
                                            "fit_endpoint_time",
                                            fitEndpointTime, fitMinTime,
                                            fitMaxTime);

      double fitSigmaX =
          fitStatusX == 0 ? std::abs(fitEndpointX.GetParameter(2)) : -1.;
      double fitSigmaY =
          fitStatusY == 0 ? std::abs(fitEndpointY.GetParameter(2)) : -1.;
      double fitSigmaZ =
          fitStatusZ == 0 ? std::abs(fitEndpointZ.GetParameter(2)) : -1.;
      double fitSigmaTime =
          fitStatusTime == 0 ? std::abs(fitEndpointTime.GetParameter(2)) : -1.;
      double fitSigmaTransverse =
          fitSigmaX >= 0. && fitSigmaZ >= 0.
              ? std::sqrt(0.5 * (fitSigmaX * fitSigmaX + fitSigmaZ * fitSigmaZ))
              : -1.;

      endpointXHist.Write();
      endpointYHist.Write();
      endpointZHist.Write();
      endpointTimeHist.Write();
      fitEndpointX.Write();
      fitEndpointY.Write();
      fitEndpointZ.Write();
      fitEndpointTime.Write();

      TTree fitSummary("endpoint_fit_summary",
                       "Gaussian fit summary for collected electron endpoints");
      int fitStatusSummaryX = fitStatusX;
      int fitStatusSummaryY = fitStatusY;
      int fitStatusSummaryZ = fitStatusZ;
      int fitStatusSummaryTime = fitStatusTime;
      double fitMeanX = fitStatusX == 0 ? fitEndpointX.GetParameter(1) : -1.;
      double fitMeanY = fitStatusY == 0 ? fitEndpointY.GetParameter(1) : -1.;
      double fitMeanZ = fitStatusZ == 0 ? fitEndpointZ.GetParameter(1) : -1.;
      double fitMeanTime =
          fitStatusTime == 0 ? fitEndpointTime.GetParameter(1) : -1.;
      double fitAmplitudeX =
          fitStatusX == 0 ? fitEndpointX.GetParameter(0) : -1.;
      double fitAmplitudeY =
          fitStatusY == 0 ? fitEndpointY.GetParameter(0) : -1.;
      double fitAmplitudeZ =
          fitStatusZ == 0 ? fitEndpointZ.GetParameter(0) : -1.;
      double fitAmplitudeTime =
          fitStatusTime == 0 ? fitEndpointTime.GetParameter(0) : -1.;
      fitSummary.Branch("statusX", &fitStatusSummaryX);
      fitSummary.Branch("statusY", &fitStatusSummaryY);
      fitSummary.Branch("statusZ", &fitStatusSummaryZ);
      fitSummary.Branch("statusTime", &fitStatusSummaryTime);
      fitSummary.Branch("amplitudeX", &fitAmplitudeX);
      fitSummary.Branch("amplitudeY", &fitAmplitudeY);
      fitSummary.Branch("amplitudeZ", &fitAmplitudeZ);
      fitSummary.Branch("amplitudeTime", &fitAmplitudeTime);
      fitSummary.Branch("meanX", &fitMeanX);
      fitSummary.Branch("meanY", &fitMeanY);
      fitSummary.Branch("meanZ", &fitMeanZ);
      fitSummary.Branch("meanTime", &fitMeanTime);
      fitSummary.Branch("sigmaX", &fitSigmaX);
      fitSummary.Branch("sigmaY", &fitSigmaY);
      fitSummary.Branch("sigmaZ", &fitSigmaZ);
      fitSummary.Branch("sigmaTransverse", &fitSigmaTransverse);
      fitSummary.Branch("sigmaTime", &fitSigmaTime);
      fitSummary.Branch("fitMinX", &fitMinX);
      fitSummary.Branch("fitMaxX", &fitMaxX);
      fitSummary.Branch("fitMinY", &fitMinY);
      fitSummary.Branch("fitMaxY", &fitMaxY);
      fitSummary.Branch("fitMinZ", &fitMinZ);
      fitSummary.Branch("fitMaxZ", &fitMaxZ);
      fitSummary.Branch("fitMinTime", &fitMinTime);
      fitSummary.Branch("fitMaxTime", &fitMaxTime);
      fitSummary.Fill();
      fitSummary.Write();

      auto drawFitParameters = [](const TF1& fit, const int status,
                                  const double fitMin, const double fitMax) {
        TPaveText* text = new TPaveText(0.14, 0.68, 0.44, 0.88, "NDC");
        text->SetFillColor(0);
        text->SetBorderSize(1);
        text->SetTextAlign(12);
        text->SetTextSize(0.035);
        text->AddText(Form("fit status = %d", status));
        text->AddText(Form("range = [%.4g, %.4g]", fitMin, fitMax));
        if (status == 0) {
          text->AddText(Form("A = %.4g", fit.GetParameter(0)));
          text->AddText(Form("#mu = %.4g", fit.GetParameter(1)));
          text->AddText(Form("#sigma = %.4g", std::abs(fit.GetParameter(2))));
        } else {
          text->AddText("fit failed");
        }
        text->Draw();
      };

      TCanvas* cEndpoints = new TCanvas("endpoint_distributions", "", 1000, 800);
      cEndpoints->Divide(2, 2);
      cEndpoints->cd(1);
      endpointXHist.Draw();
      if (fitStatusX == 0) fitEndpointX.Draw("same");
      drawFitParameters(fitEndpointX, fitStatusX, fitMinX, fitMaxX);
      cEndpoints->cd(2);
      endpointYHist.Draw();
      if (fitStatusY == 0) fitEndpointY.Draw("same");
      drawFitParameters(fitEndpointY, fitStatusY, fitMinY, fitMaxY);
      cEndpoints->cd(3);
      endpointZHist.Draw();
      if (fitStatusZ == 0) fitEndpointZ.Draw("same");
      drawFitParameters(fitEndpointZ, fitStatusZ, fitMinZ, fitMaxZ);
      cEndpoints->cd(4);
      endpointTimeHist.Draw();
      if (fitStatusTime == 0) fitEndpointTime.Draw("same");
      drawFitParameters(fitEndpointTime, fitStatusTime, fitMinTime, fitMaxTime);
      writeCanvasWithPngImage(cEndpoints, "endpoint_distributions",
                              "endpoint_distributions_png");

      TTree diffusionTree("electron_diffusion",
                          "Electron diffusion during avalanche");
      int diffusionEvent = 0;
      int diffusionFrame = 0;
      int diffusionNElectrons = 0;
      double diffusionTime = 0.;
      double meanX = 0.;
      double meanY = 0.;
      double meanZ = 0.;
      double sigmaX = 0.;
      double sigmaY = 0.;
      double sigmaZ = 0.;
      double sigmaT = 0.;
      diffusionTree.Branch("event", &diffusionEvent);
      diffusionTree.Branch("frame", &diffusionFrame);
      diffusionTree.Branch("time", &diffusionTime);
      diffusionTree.Branch("nElectrons", &diffusionNElectrons);
      diffusionTree.Branch("meanX", &meanX);
      diffusionTree.Branch("meanY", &meanY);
      diffusionTree.Branch("meanZ", &meanZ);
      diffusionTree.Branch("sigmaX", &sigmaX);
      diffusionTree.Branch("sigmaY", &sigmaY);
      diffusionTree.Branch("sigmaZ", &sigmaZ);
      diffusionTree.Branch("sigmaT", &sigmaT);
      for (std::size_t i = 0; i < diffusionTimes.size(); ++i) {
        diffusionEvent = diffusionEvents[i];
        diffusionFrame = diffusionFrames[i];
        diffusionTime = diffusionTimes[i];
        diffusionNElectrons = diffusionElectronCounts[i];
        meanX = diffusionMeanX[i];
        meanY = diffusionMeanY[i];
        meanZ = diffusionMeanZ[i];
        sigmaX = diffusionSigmaX[i];
        sigmaY = diffusionSigmaY[i];
        sigmaZ = diffusionSigmaZ[i];
        sigmaT = diffusionSigmaT[i];
        diffusionTree.Fill();
      }
      diffusionTree.Write();

      TCanvas* cDiffusion = new TCanvas("diffusion", "", 800, 600);
      cDiffusion->SetGrid();
      TMultiGraph* diffusionGraph = new TMultiGraph(
          "diffusion_graph", "Transverse electron diffusion;time [ns];sigmaT [cm]");
      TLegend* diffusionLegend = new TLegend(0.68, 0.72, 0.88, 0.88);
      const std::vector<int> eventColours = {kRed + 1, kBlue + 1,
                                             kGreen + 2, kMagenta + 1,
                                             kCyan + 1, kOrange + 1,
                                             kViolet + 1, kAzure + 1};
      TDirectory* diffusionEventGraphDirectory =
          outputFile.mkdir("diffusion_event_graphs",
                           "Per-event transverse diffusion graphs");
      for (int event = 0; event < nEvents; ++event) {
        std::vector<double> eventTimes;
        std::vector<double> eventSigmaT;
        for (std::size_t i = 0; i < diffusionTimes.size(); ++i) {
          if (diffusionEvents[i] != event) continue;
          eventTimes.push_back(diffusionTimes[i]);
          eventSigmaT.push_back(diffusionSigmaT[i]);
        }
        if (eventTimes.empty()) continue;
        TGraph* graph = new TGraph(static_cast<int>(eventTimes.size()),
                                   eventTimes.data(), eventSigmaT.data());
        graph->SetName(("diffusion_sigma_t_event_" + std::to_string(event)).c_str());
        graph->SetLineColor(eventColours[event % eventColours.size()]);
        graph->SetLineWidth(2);
        diffusionGraph->Add(graph, "L");
        diffusionLegend->AddEntry(graph,
                                  ("event " + std::to_string(event)).c_str(),
                                  "l");
        if (diffusionEventGraphDirectory) {
          diffusionEventGraphDirectory->WriteObject(graph, graph->GetName());
        }
      }
      diffusionGraph->Draw("A");
      diffusionLegend->Draw();
      diffusionGraph->Write();
      writeCanvasWithPngImage(cDiffusion, "diffusion", "diffusion_png");

      if (plotDrift) {
        TCanvas* cd = new TCanvas("drift", "", 600, 600);
        driftView.SetCanvas(cd);
        driftView.Plot(true);
        writeCanvasWithPngImage(cd, "drift", "drift_png");
      }

      if (plotField) {
        TCanvas* cfield = new TCanvas("field", "", 600, 600);
        fieldView.SetCanvas(cfield);
        cfield->SetLeftMargin(0.16);
        fieldView.Plot("e", "zcol");
        writeCanvasWithPngImage(cfield, "field", "field_png");
      }

      outputFile.Close();
}
