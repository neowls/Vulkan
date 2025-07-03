#include "ChatUI.h"
#include "imgui.h"
#include "imgui_stdlib.h"

namespace ChatUI 
{

	State state = State::Lobby;
	std::string nickname = "Anonymous";
	std::string serverIP = "127.0.0.1";
	std::string inputMessage;

	//	�Է� ��Ŀ�� ����
	static bool focusInput = false;

    void RenderLobby(Callbacks& cb) 
    {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin(u8"ä�� �κ�");

        ImGui::Text(u8"�г���");
        ImGui::SameLine();
        ImGui::InputText("##nickname", &nickname);

        // �� �����
        if (ImGui::Button(u8"�� �����"))
        {
            if (!nickname.empty() && cb.onHost)
                cb.onHost(nickname);
            state = State::Chat;
        }
        ImGui::SameLine();

        // ����
        if (ImGui::Button(u8"�����ϱ�")) 
        {
            ImGui::OpenPopup("JoinPopup");
        }

        if (ImGui::BeginPopup("JoinPopup")) 
        {
            ImGui::InputText(u8"���� IP", &serverIP);
            if (ImGui::Button(u8"����")) 
            {
                if (!nickname.empty() && cb.onJoin)
                    cb.onJoin(nickname, serverIP);
                state = State::Chat;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"���")) 
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    void RenderChat(const std::vector<std::string>& chatMessages, Callbacks& cb) 
    {
        ImGui::Begin(u8"ä��");

        // �޽��� ����Ʈ
        ImGui::BeginChild("ChatHistory", ImVec2(0, 300), true);
        for (const auto& msg : chatMessages)
            ImGui::TextUnformatted(msg.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::Separator();

        // �Է�
        ImGui::PushItemWidth(-80);
        if (focusInput) 
        {
            ImGui::SetKeyboardFocusHere();
            focusInput = false;
        }
        bool enterPressed = ImGui::InputText("##Input", &inputMessage, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool buttonPressed = ImGui::Button(u8"����", ImVec2(70, 0));
        ImGui::PopItemWidth();

        if ((enterPressed || buttonPressed) && !inputMessage.empty()) 
        {
            if (cb.onSend) cb.onSend(inputMessage);
            inputMessage.clear();
            focusInput = true;
        }

        ImGui::End();
    }

	void Render(const std::vector<std::string>& chatMessages, Callbacks& callbacks)
	{
        if (state == State::Lobby)
            RenderLobby(callbacks);
        else
            RenderChat(chatMessages, callbacks);
	}
}