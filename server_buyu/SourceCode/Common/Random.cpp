#include "stdafx.h"
#include "Random.h"
#define W 32
#define R 16
#define P 0
#define M1 13
#define M2 9
#define M3 5

#define MAT0POS(t,v) (v^(v>>t))
#define MAT0NEG(t,v) (v^(v<<(-(t))))
#define MAT3NEG(t,v) (v<<(-(t)))
#define MAT4NEG(t,b,v) (v ^ ((v<<(-(t))) & b))

#define V0            STATE[state_i                   ]
#define VM1           STATE[(state_i+M1) & 0x0000000fU]
#define VM2           STATE[(state_i+M2) & 0x0000000fU]
#define VM3           STATE[(state_i+M3) & 0x0000000fU]
#define VRm1          STATE[(state_i+15) & 0x0000000fU]
#define VRm2          STATE[(state_i+14) & 0x0000000fU]
#define newV0         STATE[(state_i+15) & 0x0000000fU]
#define newV1         STATE[state_i                 ]
#define newVRm1       STATE[(state_i+14) & 0x0000000fU]


static unsigned int state_i = 0;
static unsigned int STATE[R];
static unsigned int z0, z1, z2;

void InitWELLRNG512a(unsigned int *init)
{
	int j;
	state_i = 0;
	for (j = 0; j < R; j++)
	{
		STATE[j] = init[j];
	}
}

//�Զ���ʼ��
struct RandomInit
{
	UINT INIT_DATA[R];
	RandomInit()
	{
		Init();
	}
	void Init()
	{
		UINT d[R];
		d[0] = GetTickCount();
		d[1] = GetTickCount();
		d[2] = ::GetCurrentThreadId();
		d[3] = ::GetCurrentProcessId();
		POINT pt;
		GetCursorPos(&pt);
		d[4] = pt.x;
		d[5] = pt.y;
		SYSTEMTIME  stt;
		::GetLocalTime(&stt);
		d[6] = stt.wDay;
		d[7] = stt.wDayOfWeek;
		d[8] = stt.wHour;
		d[9] = stt.wMilliseconds;
		d[10] = stt.wMinute;
		d[11] = stt.wMonth;
		d[12] = stt.wSecond;
		d[13] = stt.wYear;
		d[14] = GetPixel(GetDC(NULL), 534, 330);
		d[15] = GetPixel(GetDC(NULL), 870, 444);
		memcpy(INIT_DATA, d, sizeof(d));
		Reset();
	}
	void Reset()
	{
		InitWELLRNG512a(INIT_DATA);
	}
	UINT	Random()
	{
		z0 = VRm1;
		z1 = MAT0NEG(-16, V0) ^ MAT0NEG(-15, VM1);
		z2 = MAT0POS(11, VM2);
		newV1 = z1                  ^ z2;
		newV0 = MAT0NEG(-2, z0) ^ MAT0NEG(-18, z1) ^ MAT3NEG(-28, z2) ^ MAT4NEG(-5, 0xda442d24U, newV1);
		state_i = (state_i + 15) & 0x0000000fU;

		return STATE[state_i];
	}
	static RandomInit* Inst()
	{
		static RandomInit g_kRandomInit;
		return &g_kRandomInit;
	}
};


UINT RandUInt()
{
	return RandomInit::Inst()->Random();
}
void RandomReinit(bool bReset)
{
	if (bReset)
		RandomInit::Inst()->Reset();
	else
		RandomInit::Inst()->Init();
}

