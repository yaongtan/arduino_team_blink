const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>blink_web</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <style>
        body { background-color: #f4f7f6; font-family: 'Apple SD Gothic Neo', sans-serif; margin: 0; padding: 20px; color: #333; }
        .container { max-width: 1000px; margin: 0 auto; }
        h1 { text-align: center; color: #2c3e50; margin-bottom: 20px; }
        
        .diagnosis-card { background: white; border-radius: 15px; padding: 25px; box-shadow: 0 4px 15px rgba(0,0,0,0.05); margin-bottom: 30px; border-left: 10px solid #ccc; }
        .diagnosis-title { font-size: 1.6em; font-weight: bold; margin-bottom: 10px; }
        .diagnosis-text { font-size: 1.1em; line-height: 1.6; color: #555; }
        .recommendation { margin-top: 15px; padding: 15px; background-color: #f8f9fa; border-radius: 8px; font-weight: bold; border: 1px solid #eee; display: inline-block; }

        .chart-container { background: white; padding: 20px; border-radius: 15px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); margin-bottom: 20px; }
        .chart-title { text-align: center; font-weight: bold; margin-bottom: 15px; color: #555; }
        .row { display: flex; gap: 20px; }
        .col { flex: 1; }
        @media (max-width: 768px) { .row { flex-direction: column; } }

        .table-container { background: white; padding: 20px; border-radius: 15px; box-shadow: 0 2px 10px rgba(0,0,0,0.05); overflow-x: auto; }
        table { width: 100%; border-collapse: collapse; margin-top: 10px; }
        th, td { padding: 12px; text-align: center; border-bottom: 1px solid #eee; }
        th { background-color: #f8f9fa; color: #555; }
        .type-led { color: #FF6384; font-weight: bold; }
        .type-rhythm { color: #36A2EB; font-weight: bold; }
    </style>
</head>
<body>

<div class="container">
    <h1>[오늘의 분석 보고서]</h1>

    <div id="ai-message-box" class="diagnosis-card">
        <div class="diagnosis-title">데이터 분석 대기 중...</div>
        <div class="diagnosis-text">로딩 중입니다...</div>
    </div>

    <div class="chart-container">
        <div class="chart-title">일별 평균 정확도 추이 (%)</div>
        <canvas id="accuracyChart"></canvas>
    </div>

    <div class="row">
        <div class="col chart-container">
            <div class="chart-title" style="color:#FF6384;">LED 반응 속도 (초)</div>
            <canvas id="ledTimeChart"></canvas>
        </div>
        <div class="col chart-container">
            <div class="chart-title" style="color:#36A2EB;">박자 게임 소요 시간 (초)</div>
            <canvas id="rhythmTimeChart"></canvas>
        </div>
    </div>

    <div class="table-container">
        <div class="chart-title">최근 게임 기록</div>
        <table>
            <thead><tr><th>시간</th><th>게임 종류</th><th>정확도(오차)</th><th>소요 시간</th></tr></thead>
            <tbody id="logTableBody"></tbody>
        </table>
    </div>
</div>

<script>
    // 페이지 로드 시 데이터 가져오기
    window.onload = loadData;

    async function loadData() {
        try {
            // NodeMCU의 /data 경로에서 텍스트 파일 내용을 가져옴
            const response = await fetch('/data');
            const text = await response.text();
            
            // 데이터 파싱 (형식: timestamp,type,score,time)
            const lines = text.trim().split('\n');
            const logs = [];

            lines.forEach(line => {
                const parts = line.split(',');
                if (parts.length >= 4) {
                    logs.push({
                        timestamp: parseInt(parts[0])*1000,
                        game_type: parts[1],
                        accuracy: parseFloat(parts[2]),
                        play_time: parseFloat(parts[3])
                    });
                }
            });

            processData(logs);

        } catch (error) {
            console.error("데이터 로드 실패:", error);
            document.querySelector('.diagnosis-text').innerHTML = "데이터를 불러오는 데 실패했습니다.<br>NodeMCU 연결을 확인해주세요.";
        }
    }

    function processData(logs) {
        // 1. 테이블 업데이트 (최신순 10개)
        const tbody = document.getElementById('logTableBody');
        tbody.innerHTML = "";
        
        // 날짜 오름차순 정렬
        logs.sort((a, b) => a.timestamp - b.timestamp);

        // 테이블용 역순 복사본
        const recentLogs = [...logs].reverse().slice(0, 10);
        
        recentLogs.forEach(log => {
            const date = new Date(log.timestamp);
            const timeStr = date.toLocaleTimeString('ko-KR', { hour: '2-digit', minute: '2-digit' });
            const dateStr = `${date.getMonth()+1}/${date.getDate()}`;
            const typeClass = log.game_type === "LED" ? "type-led" : "type-rhythm";
            const row = `<tr><td>${dateStr} ${timeStr}</td><td class="${typeClass}">${log.game_type}</td><td>${log.accuracy}</td><td>${log.play_time}</td></tr>`;
            tbody.innerHTML += row;
        });

        // 2. 차트 데이터 가공 (일별 그룹화)
        let dailyGroups = {};
        let stats = {
            LED: { historyAcc: [], historyTime: [], todayAcc: [], todayTime: [] },
            RHYTHM: { historyAcc: [], historyTime: [], todayAcc: [], todayTime: [] }
        };
        
        const today = new Date();
        today.setHours(0,0,0,0);

        logs.forEach(log => {
            const logDate = new Date(log.timestamp);
            const dateKey = `${logDate.getMonth()+1}/${logDate.getDate()}`; // "12/22" 형식

            // 그룹화 초기화
            if (!dailyGroups[dateKey]) {
                dailyGroups[dateKey] = { LED: { acc: [], time: [] }, RHYTHM: { acc: [], time: [] } };
            }
            // 데이터 추가
            if (dailyGroups[dateKey][log.game_type]) {
                dailyGroups[dateKey][log.game_type].acc.push(log.accuracy);
                dailyGroups[dateKey][log.game_type].time.push(log.play_time);
            }

            // 진단용 데이터 분류
            const isToday = logDate >= today;
            const target = stats[log.game_type];
            if(target) {
                (isToday ? target.todayAcc : target.historyAcc).push(log.accuracy);
                (isToday ? target.todayTime : target.historyTime).push(log.play_time);
            }
        });

        // 3. 차트 그리기용 배열 생성
        const labels = Object.keys(dailyGroups);
        const getAvg = arr => arr.length ? (arr.reduce((a,b)=>a+b,0)/arr.length).toFixed(1) : null;

        const ledAccData = labels.map(d => getAvg(dailyGroups[d].LED.acc));
        const rhythmAccData = labels.map(d => getAvg(dailyGroups[d].RHYTHM.acc));
        const ledTimeData = labels.map(d => getAvg(dailyGroups[d].LED.time));
        const rhythmTimeData = labels.map(d => getAvg(dailyGroups[d].RHYTHM.time));

        drawCharts(labels, ledAccData, rhythmAccData, ledTimeData, rhythmTimeData);
        runDiagnosis(stats);
    }

    function drawCharts(labels, ledAcc, rhyAcc, ledTime, rhyTime) {
        new Chart(document.getElementById('accuracyChart'), {
            type: 'line',
            data: { labels: labels, datasets: [
                { label: 'LED 점수', borderColor: '#FF6384', backgroundColor: '#FF6384', data: ledAcc, tension: 0.3 },
                { label: '박자 오차', borderColor: '#36A2EB', backgroundColor: '#36A2EB', data: rhyAcc, tension: 0.3 }
            ]},
            options: { responsive: true }
        });

        new Chart(document.getElementById('ledTimeChart'), {
            type: 'bar',
            data: { labels: labels, datasets: [{ label: '초', backgroundColor: '#FF6384', data: ledTime }] }
        });

        new Chart(document.getElementById('rhythmTimeChart'), {
            type: 'bar',
            data: { labels: labels, datasets: [{ label: '초', backgroundColor: '#36A2EB', data: rhyTime }] }
        });
    }

    function runDiagnosis(stats) {
        const title = document.querySelector('.diagnosis-title');
        const text = document.querySelector('.diagnosis-text');
        const box = document.getElementById('ai-message-box');

        const getAvg = arr => arr.length ? arr.reduce((a,b)=>a+b,0)/arr.length : 0;
        
        // 데이터가 아예 없으면
        if (!stats.LED.todayAcc.length && !stats.RHYTHM.todayAcc.length) {
            title.innerHTML = "데이터 대기 중";
            text.innerHTML = "오늘 게임 기록이 아직 없습니다. 게임을 한 판 즐겨보세요!";
            return;
        }

        // 간단한 로직: 점수가 높으면 굿 (LED 점수는 높을수록, 리듬 오차는 낮을수록 좋음)
        const todayLed = getAvg(stats.LED.todayAcc);
        
        // 메시지 결정 로직 (단순화)
        if (todayLed >= 80) {
            title.innerHTML = "🌟 훌륭한 컨디션!";
            text.innerHTML = "LED 반응 속도와 정확도가 매우 높습니다.<br><div class='recommendation'>추천: 어려운 난이도에 도전해보세요!</div>";
            box.style.borderLeftColor = "#4CAF50";
        } else {
            title.innerHTML = "🙂 꾸준함이 중요해요";
            text.innerHTML = "조금 더 집중해보세요! 몸을 풀고 다시 시도해볼까요?<br><div class='recommendation'>추천: 스트레칭 후 재도전!</div>";
            box.style.borderLeftColor = "#FF9800";
        }
    }
</script>
</body>
</html>
)rawliteral";