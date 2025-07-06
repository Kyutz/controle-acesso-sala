// Configuração Firebase
const firebaseConfig = {
    apiKey: "AIzaSyBaII5jD_RXSrudh6oMeqdsIlFVEWXbG5I",
    authDomain: "aulaiot-59d31.firebaseapp.com",
    databaseURL: "https://aulaiot-59d31-default-rtdb.firebaseio.com",
    projectId: "aulaiot-59d31",
    storageBucket: "aulaiot-59d31.appspot.com",
    messagingSenderId: "YOUR_SENDER_ID", // Certifique-se de preencher com o valor correto
    appId: "YOUR_APP_ID" // Certifique-se de preencher com o valor correto
};

// Inicializa Firebase
firebase.initializeApp(firebaseConfig);
const database = firebase.database();

// --- Funções para Gerenciar Salas ---
function setupRoomMonitoring(roomNumber) {
    const roomPath = `salas/sala${roomNumber}`;
    const salaRef = database.ref(`${roomPath}`);
    const presentesRef = database.ref(`${roomPath}/presentes`); // Referência para os presentes
    const temperaturaRef = database.ref(`${roomPath}/temperatura`);
    const umidadeRef = database.ref(`${roomPath}/umidade`);
    const numPessoasRef = database.ref(`${roomPath}/numPessoas`);

    // Gráfico
    const tempHumCtx = document.getElementById(`tempHumChart-${roomNumber}`).getContext('2d');
    const tempHumChart = new Chart(tempHumCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperatura (°C)',
                data: [],
                borderColor: 'rgb(231, 76, 60)',
                tension: 0.1,
                fill: false
            },
            {
                label: 'Umidade (%)',
                data: [],
                borderColor: 'rgb(52, 152, 219)',
                tension: 0.1,
                fill: false
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    beginAtZero: false
                }
            }
        }
    });

    // Variáveis para armazenar os últimos 10 dados de temperatura e umidade
    let latestTempData = {};
    let latestUmidData = {};

    // Função para atualizar o gráfico com base nos dados mais recentes
    function updateCombinedChart() {
        const labels = [];
        const tempData = [];
        const umidData = [];

        // Coleta todos os timestamps únicos das últimas leituras
        const allTimestamps = new Set([
            ...Object.keys(latestTempData),
            ...Object.keys(latestUmidData)
        ]);

        // Converte para array e ordena para garantir a ordem cronológica
        const sortedTimestamps = Array.from(allTimestamps).sort();

        // Pega apenas os últimos 10 timestamps
        const finalTimestamps = sortedTimestamps.slice(-10);

        finalTimestamps.forEach(time => {
            labels.push(time);
            // Garante que o valor seja 'null' se não existir para o timestamp, evitando quebras na linha do gráfico
            tempData.push(latestTempData[time] !== undefined ? latestTempData[time] : null); 
            umidData.push(latestUmidData[time] !== undefined ? latestUmidData[time] : null); 
        });

        tempHumChart.data.labels = labels;
        tempHumChart.data.datasets[0].data = tempData;
        tempHumChart.data.datasets[1].data = umidData;
        tempHumChart.update();
    }

    // Monitora status da sala
    salaRef.on('value', (snapshot) => {
        const data = snapshot.val();
        const statusElement = document.getElementById(`status-${roomNumber}`);
        
        if (data && data.ocupada) {
            statusElement.textContent = 'OCUPADA';
            statusElement.className = 'status ocupada';
        } else {
            statusElement.textContent = 'LIVRE';
            statusElement.className = 'status livre';
        }
        document.getElementById(`statusUpdate-${roomNumber}`).textContent = 'Atualizado em: ' + new Date().toLocaleTimeString();
    });
    
    // Monitora contagem de pessoas
    numPessoasRef.on('value', (snapshot) => {
        const numPessoas = snapshot.val();
        document.getElementById(`numPessoas-${roomNumber}`).textContent = numPessoas !== null ? numPessoas : 0;
    });

    // Monitora lista de presentes na sala (NOVA LÓGICA CONSOLIDADA)
    presentesRef.on('value', (snapshot) => {
        const tbody = document.querySelector(`#presentes-${roomNumber} tbody`);
        tbody.innerHTML = ''; // Limpa a tabela antes de preencher

        if (snapshot.exists()) {
            snapshot.forEach((childSnapshot) => {
                const uid = childSnapshot.key; // O UID é a chave do nó
                // O valor do nó é o timestamp de entrada, que está em milissegundos
                const entradaTimestamp = parseInt(childSnapshot.val()); 
                const entradaDate = new Date(entradaTimestamp);
                
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${uid}</td>
                    <td>${entradaDate.toLocaleTimeString()}</td>
                `;
                tbody.appendChild(row);
            });
        } else {
            tbody.innerHTML = '<tr><td colspan="2">Nenhuma pessoa presente</td></tr>';
        }
    });
    
    // Monitora temperatura atual e histórico para o gráfico e tabela
    temperaturaRef.limitToLast(10).on('value', (snapshot) => {
        const tbody = document.querySelector(`#historicoTemperatura-${roomNumber} tbody`);
        tbody.innerHTML = '';
        
        latestTempData = {}; // Reinicia os dados de temperatura

        if (snapshot.exists()) {
            snapshot.forEach((childSnapshot) => {
                const time = childSnapshot.key;
                const temp = childSnapshot.val();
                latestTempData[time] = temp; // Armazena o dado

                // Adiciona ao histórico da tabela
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${time}</td>
                    <td>${temp.toFixed(1)}</td>
                `;
                tbody.appendChild(row);
            });

            // Pega o último valor para exibir como temperatura atual
            const lastTempSnapshot = snapshot.val();
            const lastTempKey = Object.keys(lastTempSnapshot).pop();
            const lastTemp = lastTempSnapshot[lastTempKey];
            document.getElementById(`temperatura-${roomNumber}`).textContent = lastTemp.toFixed(1);
        }
        updateCombinedChart(); // Atualiza o gráfico após receber os dados de temperatura
    });
    
    // Monitora umidade atual e histórico para o gráfico e tabela
    umidadeRef.limitToLast(10).on('value', (snapshot) => {
        const tbody = document.querySelector(`#historicoUmidade-${roomNumber} tbody`);
        tbody.innerHTML = '';
        
        latestUmidData = {}; // Reinicia os dados de umidade

        if (snapshot.exists()) {
            snapshot.forEach((childSnapshot) => {
                const time = childSnapshot.key;
                const umid = childSnapshot.val();
                latestUmidData[time] = umid; // Armazena o dado

                // Adiciona ao histórico da tabela
                const row = document.createElement('tr');
                row.innerHTML = `
                    <td>${time}</td>
                    <td>${umid.toFixed(1)}</td>
                `;
                tbody.appendChild(row);
            });

            // Pega o último valor para exibir como umidade atual
            const lastUmidSnapshot = snapshot.val();
            const lastUmidKey = Object.keys(lastUmidSnapshot).pop();
            const lastUmid = lastUmidSnapshot[lastUmidKey];
            document.getElementById(`umidade-${roomNumber}`).textContent = lastUmid.toFixed(1);
        }
        document.getElementById(`sensorUpdate-${roomNumber}`).textContent = 'Atualizado em: ' + new Date().toLocaleTimeString();
        updateCombinedChart(); // Atualiza o gráfico após receber os dados de umidade
    });
}

// Chamadas para configurar o monitoramento de cada sala
setupRoomMonitoring('101');
setupRoomMonitoring('102'); 

// Autenticação (opcional)
firebase.auth().signInWithEmailAndPassword('willian.souza2@estudante.ufla.br', 'Aa123Ww')
    .catch(error => {
        console.error('Erro de autenticação:', error);
    });