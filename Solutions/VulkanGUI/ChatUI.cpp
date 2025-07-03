#include "ChatUI.h"
#include "imgui.h"
#include "imgui_stdlib.h"

namespace ChatUI 
{

	State state = State::Lobby;
	std::string nickname = "Anonymous";
	std::string serverIP = "127.0.0.1";
	std::string inputMessage;

	//	입력 포커스 제어
	static bool focusInput = false;

    void RenderLobby(Callbacks& cb) 
    {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin(u8"채팅 로비");

        ImGui::Text(u8"닉네임");
        ImGui::SameLine();
        ImGui::InputText("##nickname", &nickname);

        // 방 만들기
        if (ImGui::Button(u8"방 만들기"))
        {
            if (!nickname.empty() && cb.onHost)
                cb.onHost(nickname);
            state = State::Chat;
        }
        ImGui::SameLine();

        // 참가
        if (ImGui::Button(u8"참가하기")) 
        {
            ImGui::OpenPopup("JoinPopup");
        }

        if (ImGui::BeginPopup("JoinPopup")) 
        {
            ImGui::InputText(u8"서버 IP", &serverIP);
            if (ImGui::Button(u8"접속")) 
            {
                if (!nickname.empty() && cb.onJoin)
                    cb.onJoin(nickname, serverIP);
                state = State::Chat;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(u8"취소")) 
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::End();
    }

    void RenderChat(const std::vector<std::string>& chatMessages, Callbacks& cb) 
    {
        ImGui::Begin(u8"채팅");

        // 메시지 리스트
        ImGui::BeginChild("ChatHistory", ImVec2(0, 300), true);
        for (const auto& msg : chatMessages)
            ImGui::TextUnformatted(msg.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::Separator();

        // 입력
        ImGui::PushItemWidth(-80);
        if (focusInput) 
        {
            ImGui::SetKeyboardFocusHere();
            focusInput = false;
        }
        bool enterPressed = ImGui::InputText("##Input", &inputMessage, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool buttonPressed = ImGui::Button(u8"전송", ImVec2(70, 0));
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