//�����̨���� Ҳ�漰�����ʵĿ���
//�����̨ ����Ʒ��� Ҳ��VIP��� Ҳ���¿����
#pragma once
class CRoleEx;
class CConfig;
class RoleLauncherManager
{
public:
	RoleLauncherManager();
	virtual ~RoleLauncherManager();
	void			OnInit(CRoleEx* pRole);//����ʼ����ʱ�� Ҳ����ȫ����Ʒ�Ѿ���ȡ�ɹ��� ���ǽ���̨����������������
	void			OnAddItem(DWORD ItemID);//�������Ʒ��ʱ��
	void			OnDelItem(DWORD ItemID);//��ʧȥ��Ʒ��ʱ��
	bool			IsCanUserLauncherByID(BYTE LauncherID);//�Ƿ����ʹ��ָ������̨ ÿ�ο��ڶ���Ҫ�����жϵ�

	void			OnVipLevelChange(BYTE OldVipLevel,BYTE NewVipLevel);//��VIP�仯��ʱ��
	//void			OnMonthCardChange(BYTE OldMonthCardID, BYTE NewMonthCardID);//�¿������仯��ʱ��

	bool			IsCanUseLauncherAllTime(BYTE LauncherID);//�жϵ�ǰ���Ƿ�Ϊ���õ�
private:
	CRoleEx*		m_pRole;
	CConfig*		m_pConfig;
	DWORD			m_LauncherStates;//��̨��״̬����
	HashMap<DWORD, BYTE> m_ItemToLauncherMap;
};