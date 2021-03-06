#include "Framework/BlockAccumulator.hh"
#include "Framework/LSSMatrix.hh"
#include "Framework/MethodCommandProvider.hh"

#include "SpectralFD/BaseBndFaceTermComputer.hh"
#include "SpectralFD/DiffBndFaceTermRHSJacobSpectralFD.hh"
#include "SpectralFD/SpectralFD.hh"
#include "SpectralFD/SpectralFDElementData.hh"

//////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace COOLFluiD::Framework;
using namespace COOLFluiD::Common;

//////////////////////////////////////////////////////////////////////////////

namespace COOLFluiD {

    namespace SpectralFD {

//////////////////////////////////////////////////////////////////////////////

MethodCommandProvider< DiffBndFaceTermRHSJacobSpectralFD, SpectralFDMethodData, SpectralFDModule >
  DiffBndFaceTermRHSJacobSpectralFDProvider("DiffBndFaceTermRHSJacob");

//////////////////////////////////////////////////////////////////////////////

DiffBndFaceTermRHSJacobSpectralFD::DiffBndFaceTermRHSJacobSpectralFD(const std::string& name) :
  DiffBndFaceTermRHSSpectralFD(name),
  m_cellBuilder(CFNULL),
  m_volTermComputer(CFNULL),
  m_faceTermComputers(),
  m_bndFaceTermComputers(),
  m_lss(CFNULL),
  m_numJacob(CFNULL),
  m_acc(CFNULL),
  m_faces(),
  m_faceNghbrStates(),
  m_pertResUpdates(),
  m_derivResUpdates(),
  m_gradUpdates(),
  m_pertGrads(),
  m_solJacobDet(),
  m_solPntsLocalCoords(),
  m_otherFaceLocalIdxs(),
  m_isFaceOnBoundary(CFNULL),
  m_nghbrCellSide(CFNULL),
  m_currCellSide(CFNULL),
  m_faceOrients(CFNULL),
  m_faceBCIdx(CFNULL),
  m_bcStateComputers(CFNULL)
{
}

//////////////////////////////////////////////////////////////////////////////

DiffBndFaceTermRHSJacobSpectralFD::~DiffBndFaceTermRHSJacobSpectralFD()
{
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::configure ( Config::ConfigArgs& args )
{
  DiffBndFaceTermRHSSpectralFD::configure(args);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::executeOnTrs()
{
  CFAUTOTRACE;

  // set BCStateComputer in the boundary face term computer
  m_bndFaceTermComputer->setBcStateComputer(m_bcStateComputer);

  // set the data needed to compute the face terms;
  setFaceTermData();

  // get InnerCells TopologicalRegionSet
  SafePtr<TopologicalRegionSet> cellTrs = MeshDataStack::getActive()->getTrs("InnerCells");

  // get current QuadFreeBCSpectralFD TRS
  SafePtr<TopologicalRegionSet> faceTrs = getCurrentTRS();

  // get bndFacesStartIdxs from SpectralFDMethodData
  map< std::string , vector< vector< CFuint > > >&
    bndFacesStartIdxsPerTRS = getMethodData().getBndFacesStartIdxs();
  vector< vector< CFuint > > bndFacesStartIdxs = bndFacesStartIdxsPerTRS[faceTrs->getName()];

  // number of face orientations (should be the same for all TRs)
  cf_assert(bndFacesStartIdxs.size() != 0);
  const CFuint nbOrients = bndFacesStartIdxs[0].size()-1;

  // number of TRs
  const CFuint nbTRs = faceTrs->getNbTRs();
  cf_assert(bndFacesStartIdxs.size() == nbTRs);

  // get the geodata of the face builder and set the TRSs
  FaceToCellGEBuilder::GeoData& geoData = m_faceBuilder->getDataGE();
  geoData.cellsTRS = cellTrs;
  geoData.facesTRS = faceTrs;
  geoData.isBoundary = true;

  // get the geodata of the cell builder and set the TRS
  CellToFaceGEBuilder::GeoData& geoDataCB = m_cellBuilder->getDataGE();
  geoDataCB.trs = cellTrs;

  // loop over TRs
  for (CFuint iTR = 0; iTR < nbTRs; ++iTR)
  {
    // loop over different orientations
    for (m_orient = 0; m_orient < nbOrients; ++m_orient)
    {
      // start and stop index of the faces with this orientation
      const CFuint startFaceIdx = bndFacesStartIdxs[iTR][m_orient  ];
      const CFuint stopFaceIdx  = bndFacesStartIdxs[iTR][m_orient+1];

      // set the orientation of the faces
      m_bndFaceTermComputer->setFaceOrientation(m_orient);

      // loop over faces with this orientation
      for (CFuint faceID = startFaceIdx; faceID < stopFaceIdx; ++faceID)
      {
        // build the face GeometricEntity
        geoData.idx = faceID;
        m_face = m_faceBuilder->buildGE();

        // get the states in the neighbouring cell
        m_cellStates = m_face->getNeighborGeo(0)->getStates();

        // if cell is parallel updatable, compute the boundary face term
        if ((*m_cellStates)[0]->isParUpdatable())
        {
          // build the neighbouring cell
          const CFuint cellID = m_face->getNeighborGeo(0)->getID();
          geoDataCB.idx = cellID;
          m_intCell = m_cellBuilder->buildGE();

          // set the gradients
          setGradients();

          // set the current face and compute the face data in the boundary face term computer
          m_bndFaceTermComputer->setCurrentFace(m_face);
          m_bndFaceTermComputer->computeFaceData();
          m_bndFaceTermComputer->computeNeighbourCellData();

          // reconstruct the states
          m_bndFaceTermComputer->reconstructFluxPntsStates(*m_cellStates);

          // reconstruct the gradients
          m_bndFaceTermComputer->reconstructFluxPntsGradients(m_cellGrads);

          // compute the face terms and the update coefficient contributions
          m_bndFaceTermComputer->computeDiffFaceTermAndUpdateCoefContributions(m_resUpdates,m_updateCoeffContr);

          // update the rhs
          updateRHS();

          // update the contribution to the update coefficient
          addUpdateCoeffContribution();

          // get all the faces neighbouring the cell
          m_faces = m_intCell->getNeighborGeos();

          // set the local indexes of the other faces than the current boundary faces
          setOtherFacesLocalIdxs();

          // get the neigbouring states of the other faces
          setFaceNeighbourStates();

          // set data for the neighbouring cell
          setCellData();

          // compute the contribution to the jacobian
          computeJacobDiffBndFaceTerm();

          // release the cell
          m_cellBuilder->releaseGE();
        }

        // release the face
        m_faceBuilder->releaseGE();
      }
    }
  }
//   CF_DEBUG_EXIT;
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::setFaceTermData()
{
  DiffBndFaceTermRHSSpectralFD::setFaceTermData();

  // get the local spectral FD data
  vector< SpectralFDElementData* >& sdLocalData = getMethodData().getSDLocalData();

  // get solution point local coordinates
  m_solPntsLocalCoords = sdLocalData[0]->getSolPntsLocalCoords();

  // set the data in the face term computers
  for (CFuint iFace = 0; iFace < m_faceTermComputers.size(); ++iFace)
  {
    m_faceTermComputers[iFace]->setFaceTermData();
  }

  // set the data in the boundary face term computers
  for (CFuint iFace = 0; iFace < m_bndFaceTermComputers.size(); ++iFace)
  {
    m_bndFaceTermComputers[iFace]->setFaceTermData();
  }

  // set the data in the volume term computer
  m_volTermComputer->setVolumeTermData(0);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::setOtherFacesLocalIdxs()
{
  // get face ID of current boundary face
  const CFuint currFaceID = m_face->getID();

  // loop over the faces of the boundary cell
  const CFuint nbrFaces = m_intCell->nbNeighborGeos();
  cf_assert(m_faces->size() == nbrFaces);
  CFuint iFace = 0;
  for (CFuint faceIdx = 0; faceIdx < nbrFaces; ++faceIdx)
  {
    if ((*m_faces)[faceIdx]->getID() != currFaceID)
    {
      m_otherFaceLocalIdxs[iFace] = faceIdx;
      ++iFace;
    }
  }
  cf_assert(iFace == m_otherFaceLocalIdxs.size());
  cf_assert(iFace == nbrFaces-1);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::setFaceNeighbourStates()
{
  // get neighbour states of other faces
  const CFuint nbrOtherFaces = m_otherFaceLocalIdxs.size();
  for (CFuint iFace = 0; iFace < nbrOtherFaces; ++iFace)
  {
    // get local face index
    const CFuint faceIdx = m_otherFaceLocalIdxs[iFace];

    // get neigbouring states
    const CFuint nbrFaceNeighbours = (*m_isFaceOnBoundary)[faceIdx] ? 1 : 2;
    for (CFuint iSide = 0; iSide < nbrFaceNeighbours; ++iSide)
    {
      GeometricEntity* cell = (*m_faces)[faceIdx]->getNeighborGeo(iSide);
      m_faceNghbrStates[iFace][iSide] = cell->getStates();
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::setCellData()
{
  // compute solution points Jacobian determinants
  m_solJacobDet =
      m_intCell->computeGeometricShapeFunctionJacobianDeterminant(*m_solPntsLocalCoords);

  // set the cell and compute the cell data in the volume term computer
  m_volTermComputer->setCurrentCell(m_intCell);
  m_volTermComputer->computeCellData();
  m_volTermComputer->reconstructStates(*m_cellStates);

  // set orientation or boundary condition state computers of the other faces
  const CFuint nbrOtherFaces = m_otherFaceLocalIdxs.size();
  for (CFuint iFace = 0; iFace < nbrOtherFaces; ++iFace)
  {
    // get local face index
    const CFuint faceIdx = m_otherFaceLocalIdxs[iFace];

    // set orientation or boundary condition
    if ((*m_isFaceOnBoundary)[faceIdx])
    {
      // set BC state computer
      m_bndFaceTermComputers[iFace]->setBcStateComputer((*m_bcStateComputers)[(*m_faceBCIdx)[faceIdx]]);

      // set the orientation of the face
      m_bndFaceTermComputers[iFace]->setFaceOrientation(faceIdx);

      // set the face in the boundary face term computer
      m_bndFaceTermComputers[iFace]->setCurrentFace((*m_faces)[faceIdx]);

      // compute the face data in the boundary face term computer
      m_bndFaceTermComputers[iFace]->computeFaceData();

//       // compute the neighbouring cell data in the boundary face term computer
//       m_bndFaceTermComputers[iFace]->computeNeighbourCellData();

      // reconstruct the states in the face flux points
      m_bndFaceTermComputers[iFace]->reconstructFluxPntsStates(*m_cellStates);
    }
    else
    {
      // set the orientation of the face
      m_faceTermComputers[iFace]->setFaceOrientation((*m_faceOrients)[faceIdx]);

      // set the face in the face term computer
      m_faceTermComputers[iFace]->setCurrentFace((*m_faces)[faceIdx]);

      // compute the face data in the face term computer
      m_faceTermComputers[iFace]->computeFaceData();

//       // compute the neighbouring cell data in the face term computer
//       m_faceTermComputers[iFace]->computeNeighbourCellData();

      // reconstruct the states in the face flux points
      m_faceTermComputers[iFace]->reconstructFluxPntsStates(m_faceNghbrStates[iFace]);
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::backupAndReconstructOtherFacesAndCellPhysVars(const CFuint iVar)
{
  // face term computers
  const CFuint nbrOtherFaces = m_otherFaceLocalIdxs.size();
  for (CFuint iFace = 0; iFace < nbrOtherFaces; ++iFace)
  {
    // get local face index
    const CFuint faceIdx = m_otherFaceLocalIdxs[iFace];

    // backup and reconstruct physical variable
    if ((*m_isFaceOnBoundary)[faceIdx])
    {
      // reconstruct the states in the face flux points
      m_bndFaceTermComputers[iFace]->backupAndReconstructPhysVar(iVar,*m_cellStates);
    }
    else
    {
      m_faceTermComputers[iFace]->backupAndReconstructPhysVar((*m_currCellSide)[faceIdx],iVar,
                                                              *m_cellStates);
    }
  }

  // volume term computer
  m_volTermComputer->backupAndReconstructPhysVar(iVar,*m_cellStates);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::restoreOtherFacesAndCellPhysVars(const CFuint iVar)
{
  // face term computers
  const CFuint nbrOtherFaces = m_otherFaceLocalIdxs.size();
  for (CFuint iFace = 0; iFace < nbrOtherFaces; ++iFace)
  {
    // get local face index
    const CFuint faceIdx = m_otherFaceLocalIdxs[iFace];

    // restore physical variable
    if ((*m_isFaceOnBoundary)[faceIdx])
    {
      m_bndFaceTermComputers[iFace]->restorePhysVar(iVar);
    }
    else
    {
      m_faceTermComputers[iFace]->restorePhysVar((*m_currCellSide)[faceIdx],iVar);
    }
  }

  // volume term computer
  m_volTermComputer->restorePhysVar(iVar);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::computePerturbedGradients()
{
  // compute the internal contribution to the gradients
  m_volTermComputer->computeGradientVolumeTerm(m_gradUpdates[0]);
  const CFuint nbrSolPnts = m_cellGrads.size();
  for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
  {
    for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
    {
      (*m_pertGrads[iSol])[iEq] = m_gradUpdates[0][iSol][iEq];
    }
  }

  // compute other face contributions to the gradients
  const CFuint nbrOtherFaces = m_otherFaceLocalIdxs.size();
  for (CFuint iFace = 0; iFace < nbrOtherFaces; ++iFace)
  {
    // get local face index
    const CFuint faceIdx = m_otherFaceLocalIdxs[iFace];

    if ((*m_isFaceOnBoundary)[faceIdx])
    {
      // compute the boundary face contribution to the gradients
      m_bndFaceTermComputers[iFace]->computeGradientFaceTerm(m_gradUpdates[0]);

      // add the contribution to the gradients
      for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
      {
        for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
        {
          (*m_pertGrads[iSol])[iEq] += m_gradUpdates[0][iSol][iEq];
        }
      }
    }
    else
    {
      // compute the boundary face contribution to the gradients
      m_faceTermComputers[iFace]->computeGradientFaceTerm(m_gradUpdates);

      // cell side with respect to this face
      const CFuint cellSide = (*m_currCellSide)[faceIdx];

      // add the contribution to the gradients
      for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
      {
        for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
        {
          (*m_pertGrads[iSol])[iEq] += m_gradUpdates[cellSide][iSol][iEq];
        }
      }
    }
  }

  // compute current boundary face contribution to the gradients
  m_bndFaceTermComputer->computeGradientFaceTerm(m_gradUpdates[0]);

  // add the contribution to the gradients and divide by Jacobian determinants
  for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
  {
    const CFreal invJacobDet = 1.0/m_solJacobDet[iSol];
    for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
    {
      (*m_pertGrads[iSol])[iEq] += m_gradUpdates[0][iSol][iEq];
      (*m_pertGrads[iSol])[iEq] *= invJacobDet;
    }
  }
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::computeJacobDiffBndFaceTerm()
{
  // get number of solution points in the cell
  const CFuint nbrSolPnts = m_cellStates->size();

  // get residual factor
  const CFreal resFactor = getMethodData().getResFactor();

  // dereference accumulator
  BlockAccumulator& acc = *m_acc;

  // set block row and column indices
  for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
  {
    acc.setRowColIndex(iSol,(*m_cellStates)[iSol]->getLocalID());
  }

  // loop over the states in the internal cell to perturb the states
  for (CFuint iSolPert = 0; iSolPert < nbrSolPnts; ++iSolPert)
  {
    // dereference state
    State& pertState = *(*m_cellStates)[iSolPert];

    // loop over the variables in the state
    for (CFuint iEqPert = 0; iEqPert < m_nbrEqs; ++iEqPert)
    {
      // perturb physical variable in state
      m_numJacob->perturb(iEqPert,pertState[iEqPert]);

      // backup and reconstruct physical variable in the boundary face flux points
      // and reconstruct the ghost states
      m_bndFaceTermComputer->backupAndReconstructPhysVar(iEqPert,*m_cellStates);

      // backup and reconstruct physical variable in the other face flux points
      // and in the cell flux points
      backupAndReconstructOtherFacesAndCellPhysVars(iEqPert);

      // recompute the cell gradients
      computePerturbedGradients();

      // reconstruct the boundary face gradients
      m_bndFaceTermComputer->reconstructFluxPntsGradients(m_pertGrads);

      // compute the perturbed boundary face term
      m_bndFaceTermComputer->computeDiffFaceTerm(m_pertResUpdates);

      // compute the finite difference derivative of the face term
      m_numJacob->computeDerivative(m_pertResUpdates,m_resUpdates,m_derivResUpdates);

      // multiply residual update derivatives with residual factor
      m_derivResUpdates *= resFactor;

      // add the derivative of the residual updates to the accumulator
      CFuint resUpdIdx = 0;
      for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol, resUpdIdx += m_nbrEqs)
      {
        acc.addValues(iSol,iSolPert,iEqPert,&m_derivResUpdates[resUpdIdx]);
      }

      // restore physical variable in state
      m_numJacob->restore(pertState[iEqPert]);

      // restore physical variable in the boundary face flux points
      m_bndFaceTermComputer->restorePhysVar(iEqPert);

      // restore physical variable in the other face flux points
      // and in the cell flux points
      restoreOtherFacesAndCellPhysVars(iEqPert);
    }
  }
//   acc.printToScreen();

  if (getMethodData().doComputeJacobian())
  {
    // add the values to the jacobian matrix
    m_lss->getMatrix()->addValues(acc);
  }

  // reset to zero the entries in the block accumulator
  acc.reset();
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::setup()
{
  CFAUTOTRACE;

  DiffBndFaceTermRHSSpectralFD::setup();

  // get CellToFaceGeBuilder
  m_cellBuilder      = getMethodData().getCellBuilder();
  m_isFaceOnBoundary = m_cellBuilder->getGeoBuilder()->getIsFaceOnBoundary();
  m_nghbrCellSide    = m_cellBuilder->getGeoBuilder()->getNeighbrCellSide ();
  m_currCellSide     = m_cellBuilder->getGeoBuilder()->getCurrentCellSide ();
  m_faceOrients      = m_cellBuilder->getGeoBuilder()->getFaceOrient      ();
  m_faceBCIdx        = m_cellBuilder->getGeoBuilder()->getFaceBCIdx       ();

  // get the volume term computer
  m_volTermComputer = getMethodData().getVolTermComputer();

  // get BC state computers
  m_bcStateComputers = getMethodData().getBCStateComputers();

  // get the linear system solver
  m_lss = getMethodData().getLinearSystemSolver()[0];

  // get the numerical Jacobian computer
  m_numJacob = getMethodData().getNumericalJacobian();

  // get number of equations and dimensionality
  const CFuint dim    = PhysicalModelStack::getActive()->getDim();

  // get the local spectral FD data
  vector< SpectralFDElementData* >& sdLocalData = getMethodData().getSDLocalData();
  cf_assert(sdLocalData.size() > 0);

  // number of solution points in a cell
  const CFuint nbrSolPnts = sdLocalData[0]->getNbrOfSolPnts();

  // get the number of faces in a cell
  const CFuint nbrFaces = sdLocalData[0]->getNbrCellFaces();
  const CFuint nbrFacesM1 = nbrFaces - 1;

  // get face term computers and boundary face term computers
  m_faceTermComputers   .resize(nbrFacesM1);
  m_bndFaceTermComputers.resize(nbrFacesM1);
  for (CFuint iFace = 0; iFace < nbrFacesM1; ++iFace)
  {
    m_faceTermComputers   [iFace] = getMethodData().getAdditionalFaceTermComputer   (iFace);
    m_bndFaceTermComputers[iFace] = getMethodData().getAdditionalBndFaceTermComputer(iFace);
  }

  // create blockaccumulator
  m_acc.reset(m_lss->createBlockAccumulator(nbrSolPnts,nbrSolPnts,m_nbrEqs));

  // resize m_otherFaceLocalIdxs
  m_otherFaceLocalIdxs.resize(nbrFacesM1);

  // resize m_faceNghbrStates
  m_faceNghbrStates.resize(nbrFacesM1);
  for (CFuint iFace = 0; iFace < nbrFacesM1; ++iFace)
  {
    m_faceNghbrStates[iFace].resize(2);
  }

  // resize variables
  const CFuint nbrCellResiduals = nbrSolPnts*m_nbrEqs;
  m_pertResUpdates .resize(nbrCellResiduals);
  m_derivResUpdates.resize(nbrCellResiduals);

  // allocate memory for perturbed gradients
  m_pertGrads.resize(nbrSolPnts);
  for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
  {
    m_pertGrads[iSol] = new vector<RealVector>(m_nbrEqs);
    for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
    {
      (*m_pertGrads[iSol])[iEq].resize(dim);
    }
  }

  // resize gradient updates
  m_gradUpdates.resize(2);
  for (CFuint iSide = 0; iSide < 2; ++iSide)
  {
    m_gradUpdates[iSide].resize(nbrSolPnts);
    for (CFuint iSol = 0; iSol < nbrSolPnts; ++iSol)
    {
      m_gradUpdates[iSide][iSol].resize(m_nbrEqs);
      for (CFuint iEq = 0; iEq < m_nbrEqs; ++iEq)
      {
        m_gradUpdates[iSide][iSol][iEq].resize(dim);
      }
    }
  }

  // resize neighbouring cell solution point Jacobian determinants
  m_solJacobDet.resize(nbrSolPnts);
}

//////////////////////////////////////////////////////////////////////////////

void DiffBndFaceTermRHSJacobSpectralFD::unsetup()
{
  CFAUTOTRACE;

  for (CFuint iSol = 0; iSol < m_pertGrads.size(); ++iSol)
  {
    deletePtr(m_pertGrads[iSol]);
  }
  m_pertGrads.resize(0);

  DiffBndFaceTermRHSSpectralFD::unsetup();
}
//////////////////////////////////////////////////////////////////////////////

    } // namespace SpectralFD

} // namespace COOLFluiD
