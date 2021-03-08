#include "all.h"

Amd64Op amd64_op[NOp] = {
#define O(op, t, x) [O##op] =
#define X(nm, zf, lf) { nm, zf, lf, },
	#include "../ops.h"
};

static int
amd64_memargs(int op)
{
	return amd64_op[op].nmem;
}

Target T_amd64_sysv = {
	.gpr0 = RAX,
	.ngpr = NGPR,
	.fpr0 = XMM0,
	.nfpr = NFPR,
	.rglob = BIT(RBP) | BIT(RSP),
	.nrglob = 2,
	.rsave = amd64_sysv_rsave,
	.nrsave = {NGPS_SYSV, NFPS_SYSV},
	.rclob = amd64_sysv_rclob,
	.nrclob = NCLR_SYSV,
	.retregs = amd64_sysv_retregs,
	.argregs = amd64_sysv_argregs,
	.memargs = amd64_memargs,
	.abi = amd64_sysv_abi,
	.isel = amd64_isel,
	.emitfn = amd64_emitfn,
};

Target T_amd64_plan9 = {
	.gpr0 = RAX,
	.ngpr = NGPR,
	.fpr0 = XMM0,
	.nfpr = NFPR,
	.rglob = BIT(RSP),
	.nrglob = 1,
	.rsave = amd64_plan9_rsave,
	.nrsave = {NGPS_PLAN9, NFPS_PLAN9},
	.rclob = amd64_plan9_rclob,
	.nrclob = NCLR_PLAN9,
	.retregs = amd64_plan9_retregs,
	.argregs = amd64_plan9_argregs,
	.memargs = amd64_memargs,
	.abi = amd64_plan9_abi,
	.isel = amd64_isel,
	.emitfn = amd64_emitfn,
};
