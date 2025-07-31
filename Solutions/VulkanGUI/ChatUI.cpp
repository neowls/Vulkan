#include "ChatUI.h"
#include <windows.h>
#include <commdlg.h>
#include "imgui.h"
#include "imgui_stdlib.h"

namespace ChatUI 
{

	State state = State::Lobby;

	std::string nickname = "Anonymous";
	std::string serverIP = "127.0.0.1";
	std::string inputMessage;

    //  에러 메세지 팝업
    bool showErrorPopup = false;
    std::string errorMessage;

    static std::string selectedFile;

	//	입력 포커스 제어
	static bool focusInput = false;

    std::string OpenFileDialog()
    {
        char szFile[MAX_PATH] = { 0 };
        OPENFILENAMEA ofn = { 0 };
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = nullptr;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "모든 파일\0*.*\0이미지 파일\0*.jpg;*.jpeg;*.png;*.bmp\0";
        ofn.nFilterIndex = 1;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

        if (GetOpenFileNameA(&ofn) == TRUE)
            return std::string(szFile);
        else
            return {};
    }

    void ShowErrorPopupIfNeeded(Callbacks& cb)
    {
        if (showErrorPopup)
        {
            ImGui::OpenPopup("에러 안내");
        }

        if (ImGui::BeginPopupModal("에러 안내", NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", errorMessage.c_str());
            if (ImGui::Button("확인"))
            {
                showErrorPopup = false;
                errorMessage.clear();

                if (state == State::Chat)
                {
                    // 로비로
                    state = State::Lobby;

                    // 리소스 정리 필요시 콜백 호출
                    if (cb.onLeave)
                    {
                        cb.onLeave();
                        ImGui::CloseCurrentPopup();
                    }
                }
            }
            ImGui::EndPopup();
        }
    }

    void RenderLobby(Callbacks& cb) 
    {
        ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("채팅 로비");

        ImGui::Text("닉네임");
        ImGui::SameLine();
        ImGui::InputText("##nickname", &nickname);

        // 방 만들기
        if (ImGui::Button("방 만들기"))
        {
            if (!nickname.empty() && cb.onHost)
                cb.onHost(nickname);
            state = State::Chat;
        }
        ImGui::SameLine();

        // 참가
        if (ImGui::Button("참가하기")) 
        {
            ImGui::OpenPopup("JoinPopup");
        }

        if (ImGui::BeginPopup("JoinPopup")) 
        {
            ImGui::InputText("서버 IP", &serverIP);
            if (ImGui::Button("접속")) 
            {
                if (!nickname.empty() && cb.onJoin)
                    cb.onJoin(nickname, serverIP);
                state = State::Chat;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("취소")) 
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // 에러 안내창
        ShowErrorPopupIfNeeded(cb);

        ImGui::End();
    }

    void RenderChat(const std::vector<std::string>& chatMessages, const std::vector<std::string>& userList, Callbacks& cb)
    {
        ImGui::Begin("채팅");


        // 유저목록
        ImGui::BeginChild("UserList", ImVec2(150, 300), true);
        ImGui::Text("접속자 목록");
        ImGui::Separator();
        for (const auto& name : userList)
            ImGui::Text("%s", name.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();

        ImGui::SameLine();

        // 메시지 리스트
        ImGui::BeginChild("ChatHistory", ImVec2(0, 300), true);
        for (const auto& msg : chatMessages)
            ImGui::TextUnformatted(msg.c_str());
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
        ImGui::EndChild();


        ImGui::Separator();

        // 입력
        ImGui::PushItemWidth(-160);
        if (focusInput) 
        {
            ImGui::SetKeyboardFocusHere();
            focusInput = false;
        }
        bool enterPressed = ImGui::InputText("##Input", &inputMessage, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();

        if (ImGui::Button("+", ImVec2(30, 0)))
        {
            std::string filePath = OpenFileDialog();
            if (!filePath.empty()) {
                selectedFile = filePath;
                // 여기에 파일 전송 처리
                // 예: SendFileToServer(filePath);
            }
        }
        ImGui::PopItemWidth();

        ImGui::SameLine();
        bool buttonPressed = ImGui::Button("전송", ImVec2(70, 0));

        if ((enterPressed || buttonPressed) && !inputMessage.empty()) 
        {
            if (cb.onSend) cb.onSend(inputMessage);
            inputMessage.clear();
            focusInput = true;
        }

        ImGui::SameLine();
        // === 방 나가기 버튼 ===
        bool outButtonPressed = ImGui::Button("나가기", ImVec2(70, 0));
        if (outButtonPressed)
        {
            if (cb.onLeave) cb.onLeave();
        }
        


        // 에러 안내창
        ShowErrorPopupIfNeeded(cb);

        ImGui::End();
    }

    void ToLobby()
    {
        state = State::Lobby;
        inputMessage.clear();
    }

	void Render(const std::vector<std::string>& chatMessages, const std::vector<std::string>& userList, Callbacks& callbacks)
	{
        if (state == State::Lobby)
            RenderLobby(callbacks);
        else
            RenderChat(chatMessages, userList, callbacks);
	}
}