# SNES Emulator Multi-Agent Test Harness

## Architecture

### Agents
1. **qa-executor**: 테스트 실행, 입력 전송, 프레임 캡처
2. **result-analyzer**: 화면 분석, 패스/실패 판정
3. **state-monitor**: 에뮬레이터 상태 감시, 오류 감지
4. **report-generator**: 자동 리포트 생성

### Test Sequence
```
F:0-30:    부트 (검은 화면 예상)
F:70-90:   START 입력 (Electronics Test 진입)
F:90-600:  Electronics Test 진행
F:600-650: 통과 화면 확인
```

### Validation Rules
- **F:30**: 모두 검은색 → ✅ PASS
- **F:70-88**: START 눌러짐 → 프레임 변화 확인
- **F:90-600**: 색상 다양성 > 5 → ✅ 테스트 진행 중
- **F:600+**: 고유색상 > 100 → ✅ 통과 화면

### Metrics
- FPS: 최소 10 이상
- 프레임 변화: 10프레임 마다 최소 1회
- 에러 로그: 0개

## Execution Flow
1. Python 엔진이 stdin으로 입력 전송
2. 에뮬레이터 headless 모드로 실행
3. 프레임 캡처 (stdout 바이너리)
4. Python이 이미지 분석
5. 결과 리포트 자동 생성
