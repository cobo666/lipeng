//����ʼ�����
#pragma once
#include "Stdafx.h"
#include <list>
class CRoleEx;
class RoleManager;
class RoleMailManager  //�������ȫ�����ʼ�ϵͳ ���ǿ��Կ�ʼ������
{
public:
	RoleMailManager();
	virtual ~RoleMailManager();

	bool OnInit(CRoleEx* pUser, RoleManager* pManager);//��ʼ��
	bool OnLoadUserMailByPage();//��ȡָ��ҳ�����ʼ�
	bool OnLoadUserMailByPageResult(DBO_Cmd_LoadUserMail* pDB);
	//void OnLoadUserMailFinish();
	bool OnGetAllUserMail();
	bool OnGetUserMailContext(DWORD MailID);
	bool OnGetUserMailItem(DWORD MailID);
	bool OnDelUserMail(DWORD MailID);
	bool OnBeAddUserMail(tagRoleMail* pMail);
	bool OnBeAddUserMailResult(DBO_Cmd_AddUserMail* pMsg);
	bool OnSendUserMail(CL_Cmd_SendUserMail* pMsg);

	//����ʼ������� ֻ�����������ڵ��ʼ�
	void OnDayChange();

	bool IsLoadDB(){ return m_IsLoadDB; }

	void ResetClientInfo(){ m_IsLoadToClient = false; }

	bool  GetMailMessageStates();
private:
	void OnSendAddMailToClient(tagRoleMail* pMail);
private:
	//std::list<tagRoleMail>		m_RoleMailVec;//����ʼ��ļ��� ʹ��List����  ��ȫ�����ʼ���˳���ȡ����
	std::list<tagRoleMail*>		m_RoleMailVec;//�ʼ��ļ���
	//����
	bool						m_IsLoadToClient;
	RoleManager*				m_RoleManager;
	CRoleEx*					m_pUser;
	bool						m_IsLoadDB;

	BYTE						m_ClientMailSum;
};