//������߽�������
#pragma once
class CRoleEx;
class RoleOnlineReward
{
public:
	RoleOnlineReward();
	virtual ~RoleOnlineReward();

	void OnGetOnlineReward(CRoleEx* pRole, BYTE ID);//��ȡ���߽���
};