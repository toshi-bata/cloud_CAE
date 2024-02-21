#include "stdafx.h"
#include <vector>
#include "SAMProcess.h"
#include <stdio.h>
#include <tchar.h>
#include "..\..\ServerCommon\utilities.h"

#ifndef _DEBUG
#include "base/base.h"
#include "base/vututil.h"
#include "vis/vis.h"
#include "vis/visdata.h"
#include "vfx/vfx.h"
#include "base/license.h"
#include "CEETRON_SAM_license.h"
#endif

#ifndef _DEBUG
				   /* useful macro for parsing group activity flags */
#define VIEW_FLAG(flags,ind) (((flags) >> ((ind)-1)) & 1)

CSAMProcess::CSAMProcess(const char *workingDir, const char *CEWModelDir)
	:m_pcWorkingDir(workingDir), m_pcCEWModelDir(CEWModelDir)
{
	printf("Ceetron SAM initialized\n");
}

CSAMProcess::~CSAMProcess(void)
{
	printf("Ceetron SAM terminated\n");
}

//----------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------
void CSAMProcess::addFieldsToExport(vis_SProp *sprop)
{
	/* Add solution output*/
	//std::vector<int> resultsToExport{ SYS_RES_D, SYS_RES_XF, SYS_RES_R, SYS_RES_FORCE };
	std::vector<int> resultsToExport{ SYS_RES_D, SYS_RES_XF, SYS_RES_R, SYS_RES_FORCE, SYS_RES_S };
	vis_SPropSetValuei(sprop, SPROP_RESFILE_NUM, (Vint)resultsToExport.size());
	vis_SPropSetValueiv(sprop, SPROP_RESFILE, resultsToExport.data());

}

//----------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------
bool CSAMProcess::launchSolver(vis_Model* model, Vchar* outputFile)
{
	auto* prosolve = vfx_ProSolveBegin();

	vfx_ProSolveSetParami(prosolve, PROSOLVE_SAVEMODEL, 1);
	vfx_ProSolveSetParami(prosolve, PROSOLVE_SAVERES, 1);

	vfx_ProSolveSetParami(prosolve, PROSOLVE_SOLVER, PROSOLVE_SYMM_SPARSE);
	vfx_ProSolveSetParami(prosolve, PROSOLVE_MATRIXORDER, PROSOLVE_MATRIXORDER_METIS);
	vfx_ProSolveSetParami(prosolve, PROSOLVE_MPCTYPE, VFX_MPCTYPE_TRANSFORM);
	vfx_ProSolveSetParami(prosolve, PROSOLVE_SOLVERTYPE, PROSOLVE_SOLVERTYPE_MFP);

	if (outputFile == nullptr)
	{
		outputFile = "result_model.op2";
	}
	vfx_ProSolveSetParamc(prosolve, PROSOLVE_RESFILE, outputFile);

	//vfx_ProSolveSetParami(prosolve, PROSOLVE_RESTYPE, VDM_NASTRAN_OUTPUT2);
	//vfx_ProSolveSetParami(prosolve, PROSOLVE_NASTRANOUTPUT, 1);
	int restype = VDM_NASTRAN_OUTPUT2;
	vfx_ProSolveSetParami(prosolve, PROSOLVE_RESTYPE, restype);
	if (restype == VDM_NASTRAN_OUTPUT2) {
		vfx_ProSolveSetParami(prosolve, PROSOLVE_NASTRANOUTPUT, 1);
	}

	/* set model */
	vfx_ProSolveSetObject(prosolve, VIS_MODEL, model);

	/* execute */
	vfx_ProSolveExec(prosolve);

	int error = vfx_ProSolveError(prosolve);

	return (error == 0);
}

int CSAMProcess::PsSolidToTetMesh(const int nparts, const PK_PART_t* parts, const double edgelen, const char *activeSession)
{
	/* DevTools interface definitions */
	vis_SurfMesh *surfmesh;
	vis_TetMesh *tetmesh;
	vsy_IntHash *nodeih;
	vsy_IntHash *vertexih;
	vis_Connect *connectsrf, *connect, *connecttet;
	vsy_IntHash *bodylih, *bodyrih, *conicih;
	vsy_IntHash *finedgeih;
	vsy_IntHash *pointvertexih;
	vsy_IntVec *faceorientiv;
	Vint /*nparts,*/ geobody, geobodyrb, geobodylb;
	Vint ii, j, m, finID, finIndex, point, normalIndex, facetID, geoface;
	Vint geoedge[3], geovert[3];
	Vint length, ix[3], index;
	Vint conicid, numconic, isconic;
	Vdouble *xl, *xn, vertex[3][3], normals[3][3];
	Vdouble xo[3], axis[3], ref[3], cr, cc;
	Vint numel, numnp, numsrfel, numsrfnp, eflags[3], sense;
	Vchar buffer[256];
	Vdouble minext[3], maxext[3], diaglen/*, edgelen*/;
	int n_vertices, n_regions, n_shells, isvoid, n_faces;
	double t, tang[2][3], etang[3];
	Vint ierr, tetmeshoption, nfree;
	Vchar filnam[256];

	/* Parasolid interface definitions */
	//PK_PART_t                    *parts;
	PK_TOPOL_fctab_facet_fin_s   *facet_fin;
	PK_TOPOL_fctab_fin_fin_s     *fin_fin;
	PK_TOPOL_fctab_fin_data_s    *fin_data;
	PK_TOPOL_fctab_data_point_s  *data_point_idx;
	PK_TOPOL_fctab_data_normal_s *data_normal_idx;
	PK_TOPOL_fctab_point_vec_s   *point_vec;
	PK_TOPOL_fctab_normal_vec_s  *normal_vec;
	PK_TOPOL_fctab_facet_face_s  *facet_face;
	PK_TOPOL_fctab_fin_edge_s    *fin_edge;
	PK_TOPOL_fctab_point_topol_s *point_topol;
	PK_VERTEX_t                  *vertices;
	PK_REGION_t                  *regions;
	PK_SHELL_t                   *shells;
	PK_FACE_t                    *faces;
	PK_LOGICAL_t                 *orients, orientf;
	PK_ERROR_t                    err = PK_ERROR_no_errors;
	//PK_PART_receive_o_t           receive_opts;
	PK_CLASS_t                    eclass, sclass;
	PK_BODY_type_t                bodyType;
	PK_TOPOL_facet_2_r_t          tables;
	PK_TOPOL_facet_2_o_t          facetOptions;
	PK_GEOM_range_vector_o_t      options;
	PK_range_result_t             range_result;
	PK_range_1_r_t                range;
	PK_LOGICAL_t                  is_solid;
	PK_SURF_t                     surf;
	PK_CURVE_t                    curve;
	PK_VECTOR_t                   pt, tg;
	char* filename = NULL;
	double growthrate = 1.0;
	double maxedgealt = 100.0;
	//int c;

	//if (argc < 2) {
	//	fprintf(stderr, "Usage: %s parasolid_x_t_file\n", argv[0]);
	//	return 0;
	//}

	vsy_LicenseValidate(CEETRON_SAM_LICENSE);

	Vint isValid = 0;
	vsy_LicenseIsValid(&isValid);
	if (!isValid)
		return 1;

	//nparts = 0;
	//parts = NULL;
	numnp = numel = 0;
	numsrfel = 0;
	numsrfnp = 0;

	/* open Parasolid file */
	//PK_PART_receive_o_m(receive_opts);
	//if (strstr(argv[1], ".x_t") || strstr(argv[1], ".xmt_txt")) {
	//	receive_opts.transmit_format = PK_transmit_format_text_c;
	//}
	//else if (strstr(argv[1], ".x_b") || strstr(argv[1], ".xmt_bin")) {
	//	receive_opts.transmit_format = PK_transmit_format_binary_c;
	//}
	//else {
	//	fprintf(stderr, "Unknown file extension\n");
	//	return 1;
	//}
	///* initialize Parasolid engine */
	//parasolid_Init();
	///* load Parasolid parts */
	//err = PK_PART_receive(argv[1], &receive_opts, &nparts, &parts);
	//if (err != PK_ERROR_no_errors) {
	//	fprintf(stderr, "PK_PART_receive error= %d\n", err);
	//	return 1;
	//}
	/* set default Parasolid tesselation options */
	PK_TOPOL_facet_2_o_m(facetOptions);
	/* set default Parasolid point projection options */
	PK_GEOM_range_vector_o_m(options);

	/* initialize DevTools objects */
	surfmesh = vis_SurfMeshBegin();
	tetmesh = vis_TetMeshBegin();
	connect = vis_ConnectBegin();
	vis_ConnectPre(connect, SYS_DOUBLE);
	vertexih = vsy_IntHashBegin();

	/* initialize tesselation choice options */
	facetOptions.choice.facet_fin = true;
	facetOptions.choice.fin_data = true;
	facetOptions.choice.data_point_idx = true;
	facetOptions.choice.data_normal_idx = true;
	facetOptions.choice.point_vec = true;
	facetOptions.choice.normal_vec = true;
	facetOptions.choice.facet_face = true;
	facetOptions.choice.fin_edge = true;
	facetOptions.choice.point_topol = true;
	facetOptions.choice.facet_fin = PK_LOGICAL_true;
	facetOptions.choice.fin_fin = PK_LOGICAL_true;
	facetOptions.choice.fin_data = PK_LOGICAL_true;
	facetOptions.choice.data_point_idx = PK_LOGICAL_true;
	facetOptions.choice.data_normal_idx = PK_LOGICAL_true;
	facetOptions.choice.point_vec = PK_LOGICAL_true;
	facetOptions.choice.normal_vec = PK_LOGICAL_true;
	facetOptions.choice.facet_face = PK_LOGICAL_true;
	facetOptions.choice.fin_edge = PK_LOGICAL_true;
	facetOptions.choice.point_topol = PK_LOGICAL_true;

	/* initialize tesselation control options */
	facetOptions.control.shape = PK_facet_shape_convex_c;
	facetOptions.control.match = PK_facet_match_topol_c;
	/*
	 * The option below is used to minimize the number
	 * of edge tesselation point that are not on the edge
	 */
	facetOptions.control.quality = PK_facet_quality_improved_c;

	/* loop over all parts */
	for (int i = 0; i < nparts; i++) {
		printf("loading part %d of %d\n", i, nparts);
		PK_ENTITY_ask_class(parts[i], &eclass);
		printf("  class part : %d\n", eclass);
		if (eclass == PK_CLASS_body) {
			/* initialize IntHash for body to the left of face */
			bodylih = vsy_IntHashBegin();
			bodyrih = vsy_IntHashBegin();
			conicih = vsy_IntHashBegin();
			faceorientiv = vsy_IntVecBegin();
			numconic = 0;

			/* check for solid, sheet, or non-manifold body */
			PK_BODY_ask_type(parts[i], &bodyType);
			printf("  body type : %d\n", bodyType);

			if (bodyType == PK_BODY_type_sheet_c ||
				bodyType == PK_BODY_type_solid_c ||
				bodyType == PK_BODY_type_general_c) {

				if (bodyType == PK_BODY_type_general_c) {
					vis_SurfMeshSetParami(surfmesh, SURFMESH_NONMANIFOLD, SYS_ON);
				}
				else {
					vis_SurfMeshSetParami(surfmesh, SURFMESH_NONMANIFOLD, SYS_OFF);
				}

				/* create region->face map */
				PK_BODY_ask_regions(parts[i], &n_regions, &regions);
				printf("    regions in part: %d\n", n_regions);
				for (int ir = 0; ir < n_regions; ir++) {
					/* skip non-solid regions */
					PK_REGION_is_solid(regions[ir], &is_solid);
					printf("    regions %d is solid: %d\n", ir, is_solid);

					if (is_solid == false) {
						continue;
					}

					PK_REGION_ask_shells(regions[ir], &n_shells, &shells);
					/*
					 * The "isvoid" flag checks whether any face orientation
					 * points outwardly to the body. If all faces point
					 * inwardly SurfMeshGenerate will still succeed.  However,
					 * this problem needs to be accounted for when passing
					 * the surface mesh to TetMesh.
					 */
					isvoid = 1;
					printf("    shells in part: %d\n", n_shells);

					for (int is = 0; is < n_shells; is++) {
						PK_SHELL_ask_oriented_faces(shells[is], &n_faces,
							&faces, &orients);
						printf("      oriented faces: %d\n", n_faces);

						for (int ifa = 0; ifa < n_faces; ifa++) {
#ifdef MESH_DEBUG

							printf("      face %d with index %d orients: %d\n", faces[ifa], ifa, orients[ifa]);
#endif
							if (!orients[ifa]) {
								vsy_IntHashInsert(bodylih, faces[ifa], regions[ir]);
								isvoid = 0;
							}
							else {
								vsy_IntHashInsert(bodyrih, faces[ifa], regions[ir]);
							}
							PK_FACE_ask_oriented_surf(faces[ifa], &surf, &orientf);
							PK_ENTITY_ask_class(surf, &sclass);
							isconic = 0;
#ifdef MESH_DEBUG
							printf("      face %d class: %d\n", ifa, sclass);
#endif
							if (sclass == PK_CLASS_plane) {
								isconic = sclass;
							}
							else if (sclass == PK_CLASS_cyl) {
								isconic = sclass;
							}
							else if (sclass == PK_CLASS_cone) {
								isconic = sclass;
							}
							else if (sclass == PK_CLASS_sphere) {
								isconic = sclass;
							}
							else if (sclass == PK_CLASS_torus) {
								isconic = sclass;
							}
							if (isconic) {
								vsy_IntHashLookup(conicih, surf, &conicid);
								if (conicid == 0) {
									numconic += 1;
#ifdef MESH_DEBUG
									printf("conicid= %d, orientf= %d\n", numconic, orientf);
#endif
									vsy_IntHashInsert(conicih, surf, numconic);
									vsy_IntVecSet(faceorientiv, numconic, orientf);
								}
							}
						}
						if (n_faces) {
							PK_MEMORY_free(faces);
							PK_MEMORY_free(orients);
						}
					}
					if (n_shells) {
						PK_MEMORY_free(shells);
					}
				}
				if (n_regions) {
					PK_MEMORY_free(regions);
				}

				/* assign part number to vertices */
				numsrfnp = numsrfel = 0;
				PK_BODY_ask_vertices(parts[i], &n_vertices, &vertices);
				printf("   number of vertices: %d\n", n_vertices);
				for (j = 0; j < n_vertices; j++) {
					vsy_IntHashInsert(vertexih, vertices[j], parts[i]);
				}
				if (n_vertices) {
					PK_MEMORY_free(vertices);
				}
				/* tesselate part */
				err = PK_TOPOL_facet_2(1, &parts[i], NULL, &facetOptions, &tables);
				if (err != 0) {
					fprintf(stderr, "PK_TOPOL_facet_2 error= %d\n", err);
					return -1;
				}

				/* gather tables */
				printf("number of tables: %d \n", tables.number_of_tables);
				for (j = 0; j < tables.number_of_tables; j++) {
					printf("  table %d fctab: %d \n", j, tables.tables[j].fctab);

					if (tables.tables[j].fctab == PK_TOPOL_fctab_facet_fin_c) {
						facet_fin = tables.tables[j].table.facet_fin;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_fin_fin_c) {
						fin_fin = tables.tables[j].table.fin_fin;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_fin_data_c) {
						fin_data = tables.tables[j].table.fin_data;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_data_point_c) {
						data_point_idx = tables.tables[j].table.data_point_idx;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_data_normal_c) {
						data_normal_idx = tables.tables[j].table.data_normal_idx;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_point_vec_c) {
						point_vec = tables.tables[j].table.point_vec;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_normal_vec_c) {
						normal_vec = tables.tables[j].table.normal_vec;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_facet_face_c) {
						facet_face = tables.tables[j].table.facet_face;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_fin_edge_c) {
						fin_edge = tables.tables[j].table.fin_edge;
					}
					else if (tables.tables[j].fctab == PK_TOPOL_fctab_point_topol_c) {
						point_topol = tables.tables[j].table.point_topol;
					}
				}
				/* initialize SurfMesh */
				printf("########## Start SurfMesh ##########\n");

				printf("number of points %d \n", point_vec->length);
				printf("number of elements: %d \n", facet_fin->length / 3);
				vis_SurfMeshDef(surfmesh, point_vec->length, facet_fin->length / 3);



				/* set conic sections */
				vsy_IntHashInitIter(conicih);
				while (vsy_IntHashNextIter(conicih, &surf, &conicid), conicid) {
					vsy_IntVecGet(faceorientiv, conicid, &sense);
					if (sense == 0)  sense = -1;
#ifdef MESH_DEBUG
					printf("conicid= %d\n", conicid);
#endif
					PK_ENTITY_ask_class(surf, &sclass);
					if (sclass == PK_CLASS_plane) {
						PK_PLANE_sf_t plane_sf;
#ifdef MESH_DEBUG
						printf("PK_CLASS_plane\n");
#endif
						PK_PLANE_ask(surf, &plane_sf);
						xo[0] = plane_sf.basis_set.location.coord[0];
						xo[1] = plane_sf.basis_set.location.coord[1];
						xo[2] = plane_sf.basis_set.location.coord[2];
						axis[0] = plane_sf.basis_set.axis.coord[0];
						axis[1] = plane_sf.basis_set.axis.coord[1];
						axis[2] = plane_sf.basis_set.axis.coord[2];
						ref[0] = plane_sf.basis_set.ref_direction.coord[0];
						ref[1] = plane_sf.basis_set.ref_direction.coord[1];
						ref[2] = plane_sf.basis_set.ref_direction.coord[2];
						vis_SurfMeshSetConic(surfmesh, conicid,
							SURFMESH_CONIC_PLANE, sense, xo, axis, ref, 0., 0.);
					}
					else if (sclass == PK_CLASS_cyl) {
						PK_CYL_sf_t cyl_sf;
#ifdef MESH_DEBUG
						printf("PK_CLASS_cyl\n");
#endif
						PK_CYL_ask(surf, &cyl_sf);
						xo[0] = cyl_sf.basis_set.location.coord[0];
						xo[1] = cyl_sf.basis_set.location.coord[1];
						xo[2] = cyl_sf.basis_set.location.coord[2];
						axis[0] = cyl_sf.basis_set.axis.coord[0];
						axis[1] = cyl_sf.basis_set.axis.coord[1];
						axis[2] = cyl_sf.basis_set.axis.coord[2];
						ref[0] = cyl_sf.basis_set.ref_direction.coord[0];
						ref[1] = cyl_sf.basis_set.ref_direction.coord[1];
						ref[2] = cyl_sf.basis_set.ref_direction.coord[2];
						cr = cyl_sf.radius;
						vis_SurfMeshSetConic(surfmesh, conicid,
							SURFMESH_CONIC_CYLINDER, sense, xo, axis, ref, cr, 0.);
					}
					else if (sclass == PK_CLASS_cone) {
						PK_CONE_sf_t cone_sf;
#ifdef MESH_DEBUG
						printf("PK_CLASS_cone\n");
#endif
						PK_CONE_ask(surf, &cone_sf);
						xo[0] = cone_sf.basis_set.location.coord[0];
						xo[1] = cone_sf.basis_set.location.coord[1];
						xo[2] = cone_sf.basis_set.location.coord[2];
						axis[0] = cone_sf.basis_set.axis.coord[0];
						axis[1] = cone_sf.basis_set.axis.coord[1];
						axis[2] = cone_sf.basis_set.axis.coord[2];
						ref[0] = cone_sf.basis_set.ref_direction.coord[0];
						ref[1] = cone_sf.basis_set.ref_direction.coord[1];
						ref[2] = cone_sf.basis_set.ref_direction.coord[2];
						cr = cone_sf.semi_angle;
						vis_SurfMeshSetConic(surfmesh, conicid,
							SURFMESH_CONIC_CONE, sense, xo, axis, ref, cr, 0.);
					}
					else if (sclass == PK_CLASS_sphere) {
						PK_SPHERE_sf_t sphere_sf;
#ifdef MESH_DEBUG
						printf("PK_CLASS_sphere\n");
#endif
						PK_SPHERE_ask(surf, &sphere_sf);
						xo[0] = sphere_sf.basis_set.location.coord[0];
						xo[1] = sphere_sf.basis_set.location.coord[1];
						xo[2] = sphere_sf.basis_set.location.coord[2];
						axis[0] = sphere_sf.basis_set.axis.coord[0];
						axis[1] = sphere_sf.basis_set.axis.coord[1];
						axis[2] = sphere_sf.basis_set.axis.coord[2];
						ref[0] = sphere_sf.basis_set.ref_direction.coord[0];
						ref[1] = sphere_sf.basis_set.ref_direction.coord[1];
						ref[2] = sphere_sf.basis_set.ref_direction.coord[2];
						cr = sphere_sf.radius;
						vis_SurfMeshSetConic(surfmesh, conicid,
							SURFMESH_CONIC_SPHERE, sense, xo, axis, ref, cr, 0.);
					}
					else if (sclass == PK_CLASS_torus) {
						PK_TORUS_sf_t torus_sf;
#ifdef MESH_DEBUG
						printf("PK_CLASS_torus\n");
#endif
						PK_TORUS_ask(surf, &torus_sf);
						xo[0] = torus_sf.basis_set.location.coord[0];
						xo[1] = torus_sf.basis_set.location.coord[1];
						xo[2] = torus_sf.basis_set.location.coord[2];
						axis[0] = torus_sf.basis_set.axis.coord[0];
						axis[1] = torus_sf.basis_set.axis.coord[1];
						axis[2] = torus_sf.basis_set.axis.coord[2];
						ref[0] = torus_sf.basis_set.ref_direction.coord[0];
						ref[1] = torus_sf.basis_set.ref_direction.coord[1];
						ref[2] = torus_sf.basis_set.ref_direction.coord[2];
						cr = torus_sf.minor_radius;
						cc = torus_sf.major_radius;
						vis_SurfMeshSetConic(surfmesh, conicid,
							SURFMESH_CONIC_TORUS, sense, xo, axis, ref, cr, cc);
#ifdef MESH_DEBUG
						printf("xo= %e %e %e\n", xo[0], xo[1], xo[2]);
						printf("axis= %e %e %e\n", axis[0], axis[1], axis[2]);
						printf("ref= %e %e %e\n", ref[0], ref[1], ref[2]);
						printf("minor_radius= %e\n", torus_sf.minor_radius);
						printf("major_radius= %e\n", torus_sf.major_radius);
#endif
					}
				}
				/* map fin->edge information */
				numsrfnp = numsrfel = 0;
				finedgeih = vsy_IntHashBegin();
				printf("number of fin_edge: %d \n", fin_edge->length);
				for (ii = 0; ii < fin_edge->length; ii++) {
					vsy_IntHashInsert(finedgeih, fin_edge->data[ii].fin,
						fin_edge->data[ii].edge);
				}
				/* map point->vertex information */
				pointvertexih = vsy_IntHashBegin();
				printf("number of point_topol: %d \n", point_topol->length);
				for (ii = 0; ii < point_topol->length; ii++) {
					vsy_IntHashInsert(pointvertexih, point_topol->data[ii].point + 1,
						point_topol->data[ii].topol);
				}
				nodeih = vsy_IntHashBegin();

				/* loop over all fins */
				length = facet_fin->length;
				j = 0;
				printf("number of point_topol %d\n", length);
				for (ii = 0; ii < length; ii++) {
					finID = facet_fin->data[ii].fin;
					finIndex = fin_data->data[finID];
					point = data_point_idx->point[finIndex];
					xl = point_vec->vec[point].coord;
					vertex[j][0] = xl[0] * scaling;
					vertex[j][1] = xl[1] * scaling;
					vertex[j][2] = xl[2] * scaling;
					normalIndex = data_normal_idx->normal[finIndex];
					xn = normal_vec->vec[normalIndex].coord;
					facetID = facet_fin->data[ii].facet;
					geoface = facet_face->face[facetID];
					PK_FACE_ask_surf(geoface, &surf);
					vsy_IntHashLookup(conicih, surf, &conicid);
					vsy_IntHashLookup(finedgeih, finID, &geoedge[(j + 2) % 3]);
					vsy_IntHashLookup(nodeih, point + 1, &index);

					/* check for new node */
					if (index == 0) {
						index = ++numsrfnp;

						/* update extent */
						if (index == 1) {
							minext[0] = maxext[0] = vertex[j][0];
							minext[1] = maxext[1] = vertex[j][1];
							minext[2] = maxext[2] = vertex[j][2];
						}
						else {
							if (vertex[j][0] < minext[0])  minext[0] = vertex[j][0];
							if (vertex[j][1] < minext[1])  minext[1] = vertex[j][1];
							if (vertex[j][2] < minext[2])  minext[2] = vertex[j][2];
							if (vertex[j][0] > maxext[0])  maxext[0] = vertex[j][0];
							if (vertex[j][1] > maxext[1])  maxext[1] = vertex[j][1];
							if (vertex[j][2] > maxext[2])  maxext[2] = vertex[j][2];
						}
						vsy_IntHashInsert(nodeih, point + 1, index);

						/*
						 * Check whether node is on edge/vertex, or interior
						 * to a surface.
						 */
						vsy_IntHashLookup(pointvertexih, point + 1, &geovert[j]);
						vsy_IntHashLookup(vertexih, geovert[j], &geobody);
						if (geobody) {
							vis_SurfMeshSetPoint(surfmesh, index, vertex[j], 1);
							vis_SurfMeshSetPointAssoc(surfmesh, VIS_GEOVERT, index,
								geovert[j]);
						}
						else {
							vis_SurfMeshSetPoint(surfmesh, index, vertex[j], 0);
						}
					}
					normals[j][0] = xn[0];
					normals[j][1] = xn[1];
					normals[j][2] = xn[2];
					ix[j] = index;
					++j;
					/* every 3 fins => completed triangle */
					if (j == 3) {
#ifdef MESH_DEBUG
						printf("fin %d is on geoface facet_face->face[%d] %d\n", ii, facetID, geoface);
#endif
						++numsrfel;

						/* gather edge information */
						for (m = 1; m <= 3; m++) {
							eflags[m - 1] = 0;
							if (geoedge[m - 1]) {
								eflags[m - 1] = 1;
							}
						}
						/* insert triangle, its associations, and normals */
						vis_SurfMeshSetTri(surfmesh, numsrfel, ix, eflags);
						vis_SurfMeshSetTriAssoc(surfmesh, VIS_GEOFACE, numsrfel, SYS_FACE,
							0, geoface);
						/////////////////////// Adding support for identification of surfaces //////////////
						vis_SurfMeshSetTriAssoc(surfmesh, VIS_PROPID, numsrfel,
							SYS_ELEM, 0, geoface);
						vis_SurfMeshSetTriAssoc(surfmesh, VIS_MISCID1, numsrfel,
							SYS_FACE, 0, geoface);
						///////////////////////////////////////////////////////////////////////////////////
						vsy_IntHashLookup(bodylih, geoface, &geobodyrb);
						vsy_IntHashLookup(bodyrih, geoface, &geobodylb);
						vis_SurfMeshSetTriBack(surfmesh, numsrfel, geobodyrb, geobodylb);
						if (geobodyrb) {
							vis_SurfMeshSetTriAssoc(surfmesh, VIS_GEOBODY, numsrfel,
								SYS_ELEM, 0, geobodyrb);
							vis_SurfMeshSetTriAssoc(surfmesh, VIS_PROPID, numsrfel,
								SYS_ELEM, 0, geobodyrb);
						}
						if (geobodylb) {
							vis_SurfMeshSetTriAssoc(surfmesh, VIS_GEOBODY, numsrfel,
								SYS_ELEM, -1, geobodylb);
							vis_SurfMeshSetTriAssoc(surfmesh, VIS_PROPID, numsrfel,
								SYS_ELEM, -1, geobodylb);
						}
						vis_SurfMeshSetTriNorm(surfmesh, numsrfel, normals);
						if (conicid) {
							vis_SurfMeshSetTriConic(surfmesh, numsrfel, conicid);
						}
						/* compute edge tangents for geometry edges */
						for (m = 1; m <= 3; m++) {
							if (geoedge[m - 1]) {

								/* assign edge association */
								vis_SurfMeshSetTriAssoc(surfmesh, VIS_GEOEDGE, numsrfel,
									SYS_EDGE, m, geoedge[m - 1]);

								/* compute parameter "t" on curve */
								etang[0] = vertex[m % 3][0] - vertex[m - 1][0];
								etang[1] = vertex[m % 3][1] - vertex[m - 1][1];
								etang[2] = vertex[m % 3][2] - vertex[m - 1][2];
								PK_EDGE_ask_curve(geoedge[m - 1], &curve);

								/* first point on edge */
								pt.coord[0] = vertex[m - 1][0];
								pt.coord[1] = vertex[m - 1][1];
								pt.coord[2] = vertex[m - 1][2];
								err = PK_CURVE_parameterise_vector(curve, pt, &t);

								/*
								 * An error indicates that the point, although
								 * supposedly on the edge, was placed away from the
								 * during the tesselation. We then try to find
								 * a point on the edge that is as close as possible
								 * to the tesselation point
								 */
								if (err != 0) {
									err = PK_GEOM_range_vector(curve, pt, &options,
										&range_result, &range);
									if (err) {
										printf(
											"Unable to project point to curve, err= %d\n",
											err);
										continue;
									}
									t = range.end.parameters[0];
								}
								PK_CURVE_eval_with_tangent(curve, t, 0, &pt, &tg);

								/* make sure tangent is aligned with edge direction */
								tang[0][0] = tg.coord[0];
								tang[0][1] = tg.coord[1];
								tang[0][2] = tg.coord[2];
								if (tang[0][0] * etang[0] + tang[0][1] * etang[1] +
									tang[0][2] * etang[2] < 0.) {
									tang[0][0] = -tang[0][0];
									tang[0][1] = -tang[0][1];
									tang[0][2] = -tang[0][2];
								}

								/* likewise, second point on edge */
								pt.coord[0] = vertex[m % 3][0];
								pt.coord[1] = vertex[m % 3][1];
								pt.coord[2] = vertex[m % 3][2];
								err = PK_CURVE_parameterise_vector(curve, pt, &t);
								if (err != 0) {
									err = PK_GEOM_range_vector(curve, pt, &options,
										&range_result, &range);
									if (err) {
										printf(
											"Unable to project point to curve, err= %d\n",
											err);
										continue;
									}
									t = range.end.parameters[0];
								}

								PK_CURVE_eval_with_tangent(curve, t, 0, &pt, &tg);
								tang[1][0] = tg.coord[0];
								tang[1][1] = tg.coord[1];
								tang[1][2] = tg.coord[2];
								if (tang[1][0] * etang[0] + tang[1][1] * etang[1] +
									tang[1][2] * etang[2] < 0.) {
									tang[1][0] = -tang[1][0];
									tang[1][1] = -tang[1][1];
									tang[1][2] = -tang[1][2];
								}

								/* set tangents */
								vis_SurfMeshSetTriTang(surfmesh, numsrfel, m, tang);
							}
						}
						j = 0;
					}
				}
				vsy_IntHashEnd(nodeih);
				vsy_IntHashEnd(finedgeih);
				vsy_IntHashEnd(conicih);
				vsy_IntHashEnd(pointvertexih);
				vsy_IntVecEnd(faceorientiv);

				/* compute extent diagonal lenth for edge length */
				diaglen = sqrt((maxext[0] - minext[0])*(maxext[0] - minext[0]) +
					(maxext[1] - minext[1])*(maxext[1] - minext[1]) +
					(maxext[2] - minext[2])*(maxext[2] - minext[2]));
				// Treble edgelen = diaglen/20.;
				//edgelen = 3.0; // Treble

				vis_SurfMeshSetParamd(surfmesh, VIS_MESH_EDGELENGTH, edgelen);
				vis_SurfMeshSetParamd(surfmesh, VIS_MESH_SPANANGLE, 45.);

				/* generate parabolic triangles */
				vis_SurfMeshSetParami(surfmesh, VIS_MESH_MAXI, 2);

				/* save SurfMesh file */
				if (writesurfmesh) {
					char fileName[] = "exam52pk";
					vis_SurfMeshWrite(surfmesh, SYS_ASCII, fileName);
				}

				/* generate surface mesh and write result */
				connectsrf = vis_ConnectBegin();
				vis_ConnectPre(connectsrf, SYS_DOUBLE);
				printf("meshing srf\n");
				vis_SurfMeshGenerate(surfmesh, connectsrf);
				ierr = vis_SurfMeshError(surfmesh);
				if (ierr) {
					printf("Error generating surface mesh, part= %d\n", parts[i]);
					sprintf(filnam, "model-%d.srf", parts[i]);
					vis_SurfMeshWrite(surfmesh, SYS_ASCII, filnam);
					vis_ConnectEnd(connectsrf);
					continue;
				}
				printf("SurfMesh part= %d complete\n", parts[i]);
				vis_ConnectNumber(connectsrf, SYS_NODE, &numsrfnp);
				vis_ConnectNumber(connectsrf, SYS_ELEM, &numsrfel);
				printf("Surface number of nodes= %d\n", numsrfnp);
				printf("Surface number of elems= %d\n", numsrfel);

				char srfModelName[_MAX_PATH] = { "\0" };
				strcat(srfModelName, m_pcWorkingDir);
				strcat(srfModelName, activeSession);
				strcat(srfModelName, ".sfr");
				vis_ConnectWrite(connectsrf, SYS_BINARY, srfModelName);

				tetmeshoption = 1;

				/* test for no free edges */
				vis_SurfMeshGetInteger(surfmesh, SURFMESH_NUMFREEEDGE, &nfree);
				if (nfree) {
					tetmeshoption = 0;
				}
				if (tetmeshoption) {
					connecttet = vis_ConnectBegin();
					vis_ConnectPre(connecttet, SYS_DOUBLE);
					vis_TetMeshConnect(tetmesh, connectsrf);
					vis_TetMeshSetParamd(tetmesh, VIS_MESH_EDGELENGTH, edgelen);
					vis_TetMeshSetParamd(tetmesh, VIS_MESH_GROWTHRATE, 2.);
					/* optional write of TetMesh contents for QA
		  vis_TetMeshWrite (tetmesh,SYS_BINARY,"model1.btet");
					*/
					vis_TetMeshGenerate(tetmesh, connecttet);
					ierr = vis_TetMeshError(tetmesh);
					if (ierr) {
						printf("Error generating tet mesh, part= %d\n", parts[i]);
						sprintf(filnam, "part_%d.btet", parts[i]);
						vis_TetMeshWrite(tetmesh, SYS_BINARY, filnam);
						vis_ConnectEnd(connecttet);
						vsy_IntHashEnd(bodylih);
						vsy_IntHashEnd(bodyrih);
						continue;
					}
					printf("TetMesh part= %d complete\n", parts[i]);
					vis_ConnectAppend(connect, connecttet);
					vis_ConnectEnd(connecttet);
				}
				else {
					vis_ConnectAppend(connect, connectsrf);
				}
			}
			vsy_IntHashEnd(bodylih);
			vsy_IntHashEnd(bodyrih);
		}
	}
	/* report total number of generate nodes and elements */
	vis_ConnectNumber(connect, SYS_NODE, &numnp);
	vis_ConnectNumber(connect, SYS_ELEM, &numel);
	printf("total number of nodes= %d\n", numnp);
	printf("total number of elems= %d\n", numel);
	/* optional write of final mesh */
	if (numnp && numel) {
		char bdfModelName[_MAX_PATH] = { "\0" };
		strcat(bdfModelName, m_pcWorkingDir);
		strcat(bdfModelName, activeSession);
		strcat(bdfModelName, ".bdf");
		vis_ConnectWrite(connect, SYS_NASTRAN_BULKDATA, bdfModelName);

		char tetModelName[_MAX_PATH] = { "\0" };
		strcat(tetModelName, m_pcWorkingDir);
		strcat(tetModelName, activeSession);
		strcat(tetModelName, ".tet");
		vis_ConnectWrite(connect, SYS_BINARY, tetModelName);

		char filePath[_MAX_PATH] = { "\0" };
		strcat(filePath, m_pcCEWModelDir);
		strcat(filePath, activeSession);
		strcat(filePath, ".bdf");
		copy_file(bdfModelName, filePath);

		return 0;
	}


	vis_SurfMeshEnd(surfmesh);
	vis_TetMeshEnd(tetmesh);
	vsy_IntHashEnd(vertexih);
	vis_ConnectEnd(connect);

	return -1;
}

int CSAMProcess::Solve(const double loadPa, const double* loadVec, const double density, const double young, const double poisson, 
	const int constCnt, const PK_FACE_t* constFaces, const int loadCnt, const PK_FACE_t* loadFaces, const char *activeSession)
{
	Vint numel, numnp, numsrfel, numsrfnp;

	vsy_LicenseValidate(CEETRON_SAM_LICENSE);

	// Read tet mesh
	char tetModelName[_MAX_PATH] = { "\0" };
	strcat(tetModelName, m_pcWorkingDir);
	strcat(tetModelName, activeSession);
	strcat(tetModelName, ".tet");

	vis_Connect *connect = vis_ConnectBegin();
	vis_ConnectPre(connect, SYS_DOUBLE);
	vis_ConnectRead(connect, SYS_BINARY, tetModelName);

	vis_ConnectNumber(connect, SYS_NODE, &numnp);
	vis_ConnectNumber(connect, SYS_ELEM, &numel);
	if (0 == numnp || 0 == numel) return -1;

	// Read surface mesh
	char srfModelName[_MAX_PATH] = { "\0" };
	strcat(srfModelName, m_pcWorkingDir);
	strcat(srfModelName, activeSession);
	strcat(srfModelName, ".sfr");

	vis_Connect *connectsrf = vis_ConnectBegin();
	vis_ConnectPre(connectsrf, SYS_DOUBLE);
	vis_ConnectRead(connectsrf, SYS_BINARY, srfModelName);

	vis_ConnectNumber(connectsrf, SYS_NODE, &numsrfnp);
	vis_ConnectNumber(connectsrf, SYS_ELEM, &numsrfel);
	if (0 == numsrfnp || 0 == numsrfel) return -1;


/////////////////////////////////////////////////////////////////////////////////////
///////////////// Loads and restraints on identified surfaces ///////////////////////
	vis_ConnectKernel(connect, 0);

	/* set property identifier 1 for each element */
	for (int i = 1; i <= numel; i++) {
		vis_ConnectSetElemAssoc(connect, VIS_PROPID, i, 1);
	}
	/* instance a GridFun object */
	vis_GridFun *gridfun = vis_GridFunBegin();
	vis_ConnectGridFun(connect, gridfun);

	/* create Model object hierarchy */
	vis_Model *model = vis_ModelBegin();

	/* material and element property hash tables */
	vsy_HashTable *mphash = vsy_HashTableBegin();
	vsy_HashTable *ephash = vsy_HashTableBegin();

	/* restraint and load case hash tables */
	vsy_HashTable *rchash = vsy_HashTableBegin();
	vsy_HashTable *lchash = vsy_HashTableBegin();

	/* solution property list */
	vsy_List *splist = vsy_ListBegin();

	/* isotropic material 1 */
	vis_MProp *mprop = vis_MPropBegin();
	vis_MPropDef(mprop, SYS_MAT_ISOTROPIC);
	vis_MPropSetValued(mprop, MPROP_E, young * 1.e+6);
	vis_MPropSetValued(mprop, MPROP_NU, poisson);
	vis_MPropSetValued(mprop, MPROP_DENSITY, density * 1.e+3);
	vis_MPropSetValued(mprop, MPROP_A, 0.000254);
	vis_MPropSetValued(mprop, MPROP_TREF, 70.0);
	vsy_HashTableInsert(mphash, 1, mprop);

	/* solid property 1 */
	vis_EProp *eprop = vis_EPropBegin();
	vis_EPropDef(eprop, VIS_ELEM_SOLID);
	vis_EPropSetValuei(eprop, EPROP_MID, 1);
	vsy_HashTableInsert(ephash, 1, eprop);

	/* restraint case 1 */
	vis_RCase *rcase = vis_RCaseBegin();
	int aId;
	std::vector<int> aSurfacesToRestraint;
	//{ 546, 552, 548, 550 };
	for (int i = 0; i < constCnt; i++)
		aSurfacesToRestraint.push_back(constFaces[i]);
	for (int i = 1; i <= numsrfnp; i++) {
		for (auto aSurface : aSurfacesToRestraint) {
			vis_ConnectNodeAssoc(connectsrf, VIS_MISCID1, 1, &i, &aId);
			if (aId == aSurface) {
				vis_RCaseSetSPC(rcase, i, SYS_DOF_TX, RCASE_FIXED, NULL, 0);
				vis_RCaseSetSPC(rcase, i, SYS_DOF_TY, RCASE_FIXED, NULL, 0);
				vis_RCaseSetSPC(rcase, i, SYS_DOF_TZ, RCASE_FIXED, NULL, 0);
				//break; //nodes can belong to different surfaces so can not break 
			}
		}
	}
	vsy_HashTableInsert(rchash, 1, rcase);

	/* load node associations to query for element faces */
	std::vector<int> aSurfacesToLoad;
	//{ 340 };
	for (int i = 0; i < loadCnt; i++)
		aSurfacesToLoad.push_back(loadFaces[i]);
	vis_Group *groupnode = vis_GroupBegin();
	vis_GroupDef(groupnode, numnp, SYS_NODE, SYS_NONE);
	for (int i = 1; i <= numsrfnp; i++) {
		for (auto aSurface : aSurfacesToLoad) {
			vis_ConnectNodeAssoc(connectsrf, VIS_MISCID1, 1, &i, &aId);
			if (aId == aSurface) {
				vis_GroupSetIndex(groupnode, i, 1);
			}
		}
	}
	vis_ConnectEnd(connectsrf);

	/* build face group to query for element faces */
	vis_Group *groupface = vis_GroupBegin();
	vis_GroupDef(groupface, numel, SYS_ELEM, SYS_FACE);
	vis_ConnectFaceGroup(connect, CONNECT_CONTAINED, groupnode, groupface);

	/* load case 1, distributed load */
	vis_LCase *lcase = vis_LCaseBegin();
	vis_LCaseSetObject(lcase, VIS_GRIDFUN, gridfun);
	Vdouble v[10][3];
	for (int k = 0; k < 10; k++) {
		v[k][0] = loadVec[0] * loadPa;
		v[k][1] = loadVec[1] * loadPa;
		v[k][2] = loadVec[2] * loadPa;
	}
	int flag;
	for (int i = 1; i <= numel; i++) {
		vis_GroupGetIndex(groupface, i, &flag);
		if (flag == 0)  continue;
		for (int no = 1; no <= 6; no++) {
			if (VIEW_FLAG(flag, no)) {
				vis_LCaseSetDistdv(lcase, SYS_FACE, i, no, LCASE_TRAC, (Vdouble*)v);
			}
		}
	}
	vsy_HashTableInsert(lchash, 1, lcase);

	/* load case 2, thermal load, soak temperature of 1000. */
	//lcase = vis_LCaseBegin();
	//double temp = 1000.;
	//for (int i = 1; i <= numnp; i++) {
	//	vis_LCaseSetConcdv(lcase, i, LCASE_TEMP, &temp);
	//}
	//vsy_HashTableInsert(lchash, 2, lcase);

	/* solution step 1 : STATIC ANALYSIS*/
	vis_SProp *sprop = vis_SPropBegin();
	vis_SPropDef(sprop, SYS_SOL_STATIC);
	vis_SPropSetValuec(sprop, SPROP_TITLE, "KojiLoads");
	vis_SPropSetValuei(sprop, SPROP_ANALYSIS, SYS_ANALYSIS_STRUCTURAL);
	vis_SPropSetValuei(sprop, SPROP_RCASE, 1);
	vis_SPropSetValued(sprop, SPROP_RCASE_FACTOR, 1.);
	vis_SPropSetValuei(sprop, SPROP_LCASE_NUM, 1);
	vis_SPropSetValuei(sprop, SPROP_LCASE, 1);
	vis_SPropSetValued(sprop, SPROP_LCASE_FACTOR, 1.);

	addFieldsToExport(sprop);

	vsy_ListInsert(splist, 1, sprop);



	/* solution step 2 : THERMAL COMPUTATION (OPTIONAL)*/
// sprop = vis_SPropBegin ();
// vis_SPropDef (sprop,SYS_SOL_STATIC);
// vis_SPropSetValuei (sprop,SPROP_ANALYSIS,SYS_ANALYSIS_STRUCTURAL);
// vis_SPropSetValuei (sprop,SPROP_RCASE,1);
// vis_SPropSetValued (sprop,SPROP_RCASE_FACTOR,1.);
// vis_SPropSetValuei (sprop,SPROP_LCASE_NUM,1);
// vis_SPropSetValuei (sprop,SPROP_LCASE,2);
// vis_SPropSetValued (sprop,SPROP_LCASE_FACTOR,1.);
// vis_SPropSetValuei (sprop,SPROP_THERMALSTRAIN,SYS_ON);

// addFieldsToExport(sprop);

// vsy_ListInsert (splist,2,sprop);
				/* register Connect in Model */
	vis_ModelSetObject(model, VIS_CONNECT, connect);

	/* register property hashtables and lists in Model */
	vis_ModelSetHashTable(model, VIS_MPROP, mphash);
	vis_ModelSetHashTable(model, VIS_EPROP, ephash);
	vis_ModelSetList(model, VIS_SPROP, splist);

	/* register case hashtables in Model */
	vis_ModelSetHashTable(model, VIS_RCASE, rchash);
	vis_ModelSetHashTable(model, VIS_LCASE, lchash);
	printf("model definition complete\n");

	/*Export input file as a Nastran bulk data file (.bdf)*/
	if (writesurfmesh) {
		char fileName[] = "result_model.bdf";
		vis_ModelWrite(model, SYS_NASTRAN_BULKDATA, fileName);
	}

	/*Launch solver and output the result file as Nastran OP2*/
	char op2ModelName[_MAX_PATH] = { "\0" };
	strcat(op2ModelName, m_pcWorkingDir);
	strcat(op2ModelName, activeSession);
	strcat(op2ModelName, ".op2");

	if (launchSolver(model, (Vchar*)op2ModelName))
	{
		char filePath[_MAX_PATH] = { "\0" };
		strcat(filePath, m_pcCEWModelDir);
		strcat(filePath, activeSession);
		strcat(filePath, ".op2");
		copy_file(op2ModelName, filePath);
	}

	vis_GridFunEnd(gridfun);
	vis_GroupEnd(groupface);
	vis_GroupEnd(groupnode);

	/* delete model heirarchy */
	vsy_HashTableForEach(ephash, (void(*)(Vobject*))vis_EPropEnd);
	vsy_HashTableEnd(ephash);
	vsy_HashTableForEach(mphash, (void(*)(Vobject*))vis_MPropEnd);
	vsy_HashTableEnd(mphash);
	vsy_ListForEach(splist, (void(*)(Vobject*))vis_SPropEnd);
	vsy_ListEnd(splist);
	vsy_HashTableForEach(rchash, (void(*)(Vobject*))vis_RCaseEnd);
	vsy_HashTableEnd(rchash);
	vsy_HashTableForEach(lchash, (void(*)(Vobject*))vis_LCaseEnd);
	vsy_HashTableEnd(lchash);
	vis_ModelEnd(model);
	//////////////////////////////////////////////////////////////////////:


	//vis_ConnectEnd(connect);
	//PK_MEMORY_free(parts);

	/* stop Parasolid engine */
	//parasolid_Term();

	return 0;
}

#endif