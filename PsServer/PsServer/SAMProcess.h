#pragma once
#ifndef _DEBUG
#include "vis/vismesh.h"
#include "vdm/vdm.h"
#include "parasolid_kernel.h"
#include <string>

class CSAMProcess
{
public:
	CSAMProcess(const char *workingDir, const char *CEWModelDir);
	~CSAMProcess(void);

private:
	const double scaling = 1.0;
	const Vint writesurfmesh = 0;
	const char *m_pcCEWModelDir;
	const char *m_pcWorkingDir;

	void addFieldsToExport(vis_SProp *sprop);
	bool launchSolver(vis_Model* model, Vchar* outputFile = nullptr);

public:
	int PsSolidToTetMesh(const int nparts, const PK_PART_t* parts, const double edgelen, const char *activeSession);
	int Solve(const double loadPa, const double* loadVec, const double density, const double young, const double poisson, 
		const int constCnt, const PK_FACE_t* constFaces, const int loadCnt, const PK_FACE_t* loadFaces, const char *activeSession);

};
#endif
