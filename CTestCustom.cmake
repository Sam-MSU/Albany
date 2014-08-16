# Tests to ignore
set(CTEST_CUSTOM_TESTS_IGNORE 
SteadyHeat1D
SteadyHeat1D_Thyra 
SteadyHeat1D_Dakota 
SteadyHeat1D_Dakota_restart 
SteadyHeat1D_Analysis 
SteadyHeat1D_Analysis_restart 
SteadyHeat2D 
SteadyHeat2D_Dakota 
SteadyHeat3D 
SteadyHeat3D_10x10x10_ioss 
SteadyHeat3D_10x10x10_ascii 
SteadyHeat1DEB 
SteadyHeat2DEB 
ContinuationHeat1D 
Heat1DPeriodic 
Heat2DTriangles 
Heat1DWithSource 
SGBratu1D_SG 
SGBratu1D_SG_Transient 
SGQuad2D_SG 
MPNIQuad2D 
MPNIQuad2D_SG 
TransientHeat1D 
TransientHeat1D_Analysis 
TransientHeat2D 
HeatEigenvalues 
Heat3DPamgen 
Heat2DMMCylWithSource 
HeatQuadTri 
Ioss2D 
Ioss2D_SerialInput 
Ioss3D_SERIAL 
IossRestart 
SteadyHeat2DInternalNeumann 
SteadyHeat2DRobin 
SteadyHeat2DSS 
SteadyHeat2DSS_dudxdudy 
Helmholtz2D 
LinComprNS_2DSteady 
LinComprNS_2DUnsteadyInvPressPulse 
LinComprNS_2DUnsteadyEulerMMS 
LinComprNS_2DDrivenPulse 
LinComprNS_3DUnsteadyInvPressPulse 
LinComprNS_3DUnsteadyInvPressPulseNS 
LinComprNS_1DStandingWave 
ComprNS_2DTaylorGreenVortex 
ODE_SERIAL 
TekoPrec 
CahnHillElast2D 
NSPoiseuille2D 
NSRayleighBernard2D 
NSRayleighBernard2D_SG 
NSVortexShedding2D 
NSVortexShedding2D_Blocked 
NSVortexShedding2D_TransIRK 
PNP_test2D 
CohesiveElement 
SurfaceElement_SERIAL 
SurfaceElementLocking 
StabilizedTet4 
HydrogenKfieldBC 
TorsionBC 
Mechanics_SERIAL 
Mechanics2D_Blocked 
Mechanics2D_J2 
Mechanics2D_J2_Trac 
MechanicsPorePressureSimple_Serial 
MechanicsPorePressure_Serial 
MechanicsPorePressureLocalized_Serial 
MechanicsPorePressureParallelFlow_Serial 
MechanicsPorePressureOrthogonalFlow_Serial 
MechanicsWithHydrogen_SERIAL 
MechanicsWithHydrogenParallel_SERIAL 
MechanicsWithHydrogenOrthogonal_SERIAL 
MechanicsWithTemperatureTransientHeat2D 
MechanicsWithTemperatureThermalExpansion 
MechanicsWithTemperature 
MechanicsRigidBody 
Dynamics 
Elasticity2DTraction 
Partition 
AnisotropicHyperelasticDamage 
AnisotropicDamage 
Gurson 
Neohookean 
Subdivision_one_tet 
BoreDemo 
Catalyst 
DomainTear2D_BaseProblem 
ProjectionElasticity3D 
CapModelPlasticity3D 
StaticElasticity1D 
StaticElasticity1D_SG 
StaticElasticity1D_Dakota 
StaticElasticity1D_Analysis 
StaticElasticity1D_Analysis_Tpetra 
StaticElasticity2D 
StaticElasticity2D_Traction 
StaticElasticity2D_Dakota 
StaticElasticity3D 
StaticElasticity3D_Traction 
Elasticity3DPressureBC 
Elasticity3DPressureBC_Transient 
Elasticity2DTriangles 
TransientElasticity2D_explicit 
TransientElasticity2D_implicit 
TransientElasticity2D_lumped 
PoroElasticity2D 
PoroElasticity3D 
UnSatPoroElasticity3D 
TLPoroElasticity2D 
TLPoroElasticity3D 
ThermoElasticity2D 
ThermoElasticity3D 
ThermoMechanical3D 
ThermoMechanical3D_SplitOutput 
ThermoPoroMechanics3D 
GradientDamage 
LameStaticElasticity3D_MatrixFree 
LameStaticElasticity3D_ProbeLame 
LameMultiMaterials_SingleMaterial_SERIAL 
LameMultiMaterials_MultiMaterial_10_SERIAL 
LameMultiMaterials_MultiMaterial_100_SERIAL 
#LaplaceBeltrami:2DQuad_Cgrid_Tpetra 
#LaplaceBeltrami:2DQuad_Cgrid_TPS_Tpetra 
#LaplaceBeltrami:2DTri_Cgrid_Tpetra 
#LaplaceBeltrami:2DTri_Cgrid_TPS_Tpetra 
#LaplaceBeltrami:2DTri6_Cgrid_Tpetra 
#LaplaceBeltrami:2DTri6_Cgrid_TPS_Tpetra 
#LaplaceBeltrami:2DQuad_arc_Tpetra 
#LaplaceBeltrami:2DQuad_arc_TPS_Tpetra 
#LaplaceBeltrami:2DQuad_chev_Tpetra 
#LaplaceBeltrami:2DQuad_chev_TPS_Tpetra 
#LaplaceBeltrami:3DHex_Cgrid_Tpetra 
#LaplaceBeltrami:3DHex_Cgrid_TPS_Tpetra 
#LaplaceBeltrami:3DTet_Cgrid_Tpetra 
#LaplaceBeltrami:3DTet_Cgrid_TPS_Tpetra 
LaplaceBeltrami:2DCylMotion_DTPS 
MultiScaleRing_nobridge_SERIAL 
MechanicsTensileTet10_STK 
QCAD_Poisson_test2D_SERIAL 
QCAD_Poisson_pmoscap_1D_SERIAL 
QCAD_Poisson_pmoscap 
QCAD_Poisson_pmoscap_polygate 
QCAD_Poisson_pndiode 
QCAD_Poisson_mosdot_2D_tri6 
QCAD_Poisson_mosdot_3D 
QCAD_Poisson_mosdot_3D_tet10 
QCAD_Schrodinger_parabolic1D_SERIAL 
QCAD_Schrodinger_parabolic2D_SERIAL 
QCAD_Schrodinger_parabolic1D 
QCAD_Schrodinger_parabolic2D 
QCAD_Schrodinger_parabolic3D 
QCAD_Schrodinger_formula2D_qcad 
QCAD_Schrodinger_infiniteWall1D 
QCAD_Schrodinger_infiniteWall2D 
QCAD_Schrodinger_finiteWall1D 
ThermoElectrostatics2D 
FELIX_FO_Dome_Ascii 
FELIX_FO_Dome_Restart 
FO_MMS_SinCosGlen 
FO_MMS_SinCosZ3DPeriodicXY 
FO_MMS_SinExp_Neumann 
FO_MMS_CosExp_Basal 
FO_MMS_FO_CosExp_BasalAll 
FO_ISMIP_TestA 
FO_ISMIP_TestC 
FO_Test_Dome 
FO_Test_ConfinedShelf 
FO_Test_ConfinedShelf_DepthIntBC 
FO_Test_CircularShelf 
FO_Test_Dome_interpSurf 
FO_GIS_Gis20km 
Stokes_MMS_Poly 
Stokes_MMS_CouettePeriodicX 
Stokes_MMS_SinSinPeriodicXY 
Stokes_MMS_SinSinDirichlet 
Stokes_MMS_SinCosZ3DDirichlet 
Stokes_MMS_SinCosZ3DPeriodicXY 
Stokes_MMS_SinCosGlen 
Stokes_MMS_SinExpBasal 
Stokes_ISMIP_TestA 
Stokes_Test_Dome 
Sphere10_Serial 
TC2_Serial 
TC5_Serial 
TC6_Serial 
XZScalarAdvection 
MOR_TransientHeat2D_galerkin_exo 
MOR_TransientHeat2D_gaussnewton_exo 
MOR_TransientHeat2D_galerkin_trunc_exo 
SteadyHeat2D_perf 
TLPoroElasticity3D_perf 
Necking3D_MMODEL_perf 
Poisson_perf 
FELIX_FO_MMS_perf 
)
