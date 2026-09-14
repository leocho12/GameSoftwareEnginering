# 소녀전선 클라이언트 리소스 점검

점검일: 2026-09-10
원본: `C:\Users\leoch\Desktop\kr.txwy.and.snqx`

## 확인 결과

파일 목록에서 총 7,465개, `.ab` 번들 5,969개, 이름이 `spine.ab`로 끝나는 후보 2,232개를 확인했다. 후보에는 캐릭터 외 NPC·장치·스킨 등이 섞여 있으므로 2,232명을 뜻하지 않는다. 캐릭터 번들의 주요 위치는 `files/Android/New`다.

4식·M4A1·UMP45·HK416·G11의 기본형 번들을 읽고 UnityFS 압축 블록을 메모리에서 해제했다. 다섯 샘플의 Unity 버전은 `2018.4.36f1`, 스켈레톤 버전 문자열은 `2.1.27`이다. 직렬화된 데이터와 텍스처 스트림 `.resS`가 포함되어 있다.

| 샘플 | 내부에서 확인한 리소스 참조 | 확인한 애니메이션명 문자열 |
|---|---|---|
| 4식 | `4type.atlas.txt`, `4type.png`, `4type.skel.bytes`, `r4type.skel.bytes` | attack, move, wait, victory, victoryloop |
| M4A1 | `m4a1.atlas.txt`, `m4a1.png`, `m4a1.skel.bytes` 및 rm4a1 세트 | attack, attack2, move, wait, victory, victoryloop |
| UMP45 | `ump45.atlas.txt`, `ump45.png`, `ump45.skel.bytes` 및 rump45 세트 | attack, move, wait, victory, victoryloop |
| HK416 | `hk416.atlas.txt`, `hk416.png`, `hk416.skel.bytes` 및 rhk416 세트 | attack, move, wait, victory, victoryloop |
| G11 | `g11.atlas.txt`, `g11.png`, `g11.skel.bytes` 및 rg11 세트 | attack, move, wait, victory, victoryloop |

위 이름은 압축 해제한 번들 내 참조와 문자열 확인 결과다. 스켈레톤 구조 전체를 파싱하거나 화면에서 애니메이션을 재생한 검증은 아니다. R 접두어 파일에는 move/wait 문자열이 확인되지만 용도는 추가 확인해야 한다.

## 실제로 필요한 것

1. 같은 캐릭터·스킨의 **텍스처 + 아틀라스 + 스켈레톤**을 같은 번들 기준으로 확보한다. 기존 `GF/Sprite/4type.png`와 신규 아틀라스의 좌표·크기가 맞는지는 아직 비교하지 않았다.
2. Unity 텍스처는 실제 PNG 파일이 그대로 들어 있다는 뜻이 아니다. Texture2D와 `.resS` 스트림을 읽고 픽셀 형식을 처리해 이미지로 내보내야 한다.
3. `.atlas.txt`와 `.skel.bytes`는 TextAsset 등 실제 객체의 데이터 범위를 읽어 내보낸다. 문자열 검색 결과를 그대로 파일로 저장하는 것은 추출이 아니다.
4. 확인된 Spine 2.1.27 데이터에 맞는 파서·재생 경로를 검토한다. 현재 C++/OpenGL 렌더러는 완성 PNG 빌보드만 지원하므로 부품 조립·뼈대 애니메이션 연결은 별도 구현이 필요하다.
5. 하나의 샘플에서 대기·이동·공격과 알파 처리를 먼저 확인한 뒤 다른 인형으로 확대한다. 샘플 선정은 기술 검토용이며 원작 캐릭터를 플레이어 소대에 편성한 결정은 아니다.

`.bytes` 1,072개와 `.stc` 250개 등 다른 데이터도 존재하지만 내용은 이번에 검토하지 않았다. `setting.txt`, 로그, 계정·캐시 관련 파일은 캐릭터 연동에 필요하지 않아 읽지 않았다.

## 검증 범위

원본 변경·실행·전체 추출·외부 업로드 없이 파일 목록과 다섯 번들만 읽었다. 파일 목록의 모든 번들이 정상인지, 개별 스킨이 완전한지는 검증하지 않았다. 이번 작업에서는 게임 코드와 보안 설정을 수정하지 않았다.

## 확인한 원본 파일

### 4type

- 경로: `C:\Users\leoch\Desktop\kr.txwy.and.snqx\files\Android\New\1yYk6DCsW7HuHnFfV2gY6n8VGfs4BLE2fRO6BmTfRqxlIoN2bIztU79PA6YJYgcharacter4typespine.ab`
- 크기: 155111 bytes
- SHA-256: `99C2AB6B2AD0237C6D88F7BE46BEB175078CBDD0BDC7032DC73203EB40188C7E`

### m4a1

- 경로: `C:\Users\leoch\Desktop\kr.txwy.and.snqx\files\Android\New\DHFTzb71eTkHLuSMvk27UbMJJUk2yHkTTSSAZAW6W4RvuwjAxyd8KO7mcharacterm4a1spine.ab`
- 크기: 257732 bytes
- SHA-256: `522604DE5459A876695253C96B8478051F7A5B80EE983A992872429D5BBC1FA0`

### ump45

- 경로: `C:\Users\leoch\Desktop\kr.txwy.and.snqx\files\Android\New\Vm3BI11oVXshbNTnKc56m10RO37spfjkrIC58kJxo3uzFyXTw6rWM4vpSPtVW4characterump45spine.ab`
- 크기: 224914 bytes
- SHA-256: `B01A1230C1E096DB75B58B5FC1B26E4E13EA5D00EF3E2E2D90F68EA13C745134`

### hk416

- 경로: `C:\Users\leoch\Desktop\kr.txwy.and.snqx\files\Android\New\5N13OfS1PhjwAo6Jz96a0Q081FpWZeC6b5vtT14f2kb2tQhfRzN06ndg93ngKSMEcharacterhk416spine.ab`
- 크기: 253201 bytes
- SHA-256: `BA35936283E9EA474150937D7A99B489A215575D10B47136B275F0F3F3AFAFA4`

### g11

- 경로: `C:\Users\leoch\Desktop\kr.txwy.and.snqx\files\Android\New\dsxDntwwxOhNdFxrcLy1IB4yKC3RIGnvwrsFdtQhlNGF76ecfHxTHlSaTfDCIcharacterg11spine.ab`
- 크기: 231805 bytes
- SHA-256: `E3199A1E7EBE9E72EA7E392DCAC92C1209E24C680DA32072717FE6726731B4CF`



## 2026-09-14 원작 SD 연결 업데이트

5종 추출·30개 클립 데이터 검증과 필드·방어전 연결 코드를 작성했다. 이전 미추출 상태 기록보다 [CHARACTER_ASSET_INTEGRATION.md](CHARACTER_ASSET_INTEGRATION.md)를 우선한다. 게임 빌드·실행은 사용자 확인 대상이다. 필드 이동·승리, 방어전 배치·공격·회복·전투 불능, 투명도·그림자·건물 가림을 확인한다. NPC·적은 임시 외형이다.
