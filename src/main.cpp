#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <AutoConnect.h>
#include <FS.h>
#include <ESP8266HTTPClient.h>

#define PIN_RELE D5
#define PIN_SENSOR_SOLO A0

const char *ssid = "YOUR_WIFI";
const char *password = "YOUR_PASS";

const char *token = "";
const char *endPointSiot = "https://test.ws.siot.com/api/v1";

ESP8266WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config(ssid, password);

String header;
bool ativo = false;
bool manual = true;
int valorSensor;
int valorLido;
int umidadeAutomatico = 60;
int tempoParaFicarLigado = 30; //segundos
int tempoLigadoAutomatico = 0;
int tempoLerNovamente = 31; //segundos
int delaySendSiot = 0;

void ConfigurandoPinoSensorSolo()
{
    pinMode(PIN_SENSOR_SOLO, INPUT);
}

void ConfiguraPinoReleEIniciaDesligado()
{
    pinMode(PIN_RELE, OUTPUT);
    digitalWrite(PIN_RELE, HIGH);
}

void AtivarSelenoide()
{
    digitalWrite(PIN_RELE, LOW);
}

void DesativarSelenoide()
{
    digitalWrite(PIN_RELE, HIGH);
}

void SalvarArquivo(String state, String path)
{
    File rFile = SPIFFS.open(path, "w+");
    if (!rFile)
    {
        Serial.println("Erro ao abrir arquivo!");
    }
    else
    {
        rFile.println(state);
        Serial.print("Gravou estado: ");
        Serial.println(state);
    }
    rFile.close();
}

String LerArquivo(String path)
{
    File rFile = SPIFFS.open(path, "r");
    if (!rFile)
    {
        Serial.println("Erro ao abrir arquivo!");
    }
    String content = rFile.readStringUntil('\r');
    Serial.print("leitura de estado: ");
    Serial.println(content);
    rFile.close();
    return content;
}

void EnviarHttp(String pathUri, String body)
{
    HTTPClient http;

    http.begin(endPointSiot + pathUri);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("key-token", token);
    int httpResponseCode = http.POST(body);
    if (httpResponseCode > 0)
    {
        Serial.println(httpResponseCode);
    }
    else
    {
        Serial.print("Error on sending POST: ");
        Serial.println(httpResponseCode);
    }
    http.end();
}

void EnviarAtivo() {
    EnviarHttp("/state/sensor", "{\"idMachine\": \"1313\",\"status\": \"active\",\"date\": \"2020-05-29T12:05:20.000Z\",\"message\": \"\",\"sensorId\": \"active\",\"sensor\": \"\",\"signals\": [{\"signal\": \"active\",\"value\":true }]}");
}

void EnviarDesativado() {
    EnviarHttp("/state/sensor", "{\"idMachine\": \"1313\",\"status\": \"active\",\"date\": \"2020-05-29T12:05:20.000Z\",\"message\": \"\",\"sensorId\": \"active\",\"sensor\": \"\",\"signals\": [{\"signal\": \"active\",\"value\": false }]}");
}

void SalvarUmidade()
{
    SalvarArquivo(String(umidadeAutomatico), "/umidade.txt");
}

void SalvarManual()
{
    SalvarArquivo(String(manual), "/manual.txt");
}

void SalvarTempo()
{
    SalvarArquivo(String(tempoParaFicarLigado), "/tempoFicarLigado.txt");
}

void VerificarTempo()
{
    if (tempoLigadoAutomatico < tempoParaFicarLigado)
    {
        tempoLigadoAutomatico++;
        delay(1000);
    }
    else
    {
        DesativarSelenoide();
        ativo = false;
        tempoLigadoAutomatico = 0;
        tempoLerNovamente = 0;
    }
}

void VerificarAutomaticoEAtivar(int valorSensor)
{
    if (ativo)
    {
        VerificarTempo();
    }
    else if (tempoLerNovamente < 30)
    {
        tempoLerNovamente++;
        delay(1000);
    }
    else
    {
        tempoLerNovamente = 30;
        if (valorSensor <= umidadeAutomatico)
        {
            AtivarSelenoide();
            ativo = true;
            EnviarAtivo();
        }
        else
        {
            DesativarSelenoide();
            ativo = false;
            tempoLigadoAutomatico = 0;
            EnviarDesativado();
        }
    }
}

void LerSensorSolo()
{
    int valorLido = analogRead(PIN_SENSOR_SOLO);
    valorSensor = map(valorLido, 550, 0, 0, 100);
    Serial.print("Umidade solo: ");
    Serial.print(valorSensor);
    Serial.println("%");

    if (valorSensor < 0)
    {
        valorSensor = 0;
    }

    if (!manual)
    {
        VerificarAutomaticoEAtivar(valorSensor);
    }
}

void PaginaInicial()
{
    char content[] = R"rawliteral(<html><head><meta charset="UTF-8"/><link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"/><style>body {font-family:Arial;display:inline-block;margin:0px auto;text-align:center;width:100%;padding-top:35px;}body div div {display:flex;flex-direction:column;align-items:center;}p {font-size:25px;font-weight:bold;}button {width:90px;}h2 {font-size:36px;}</style></head><body><h2>Jardim Wifi</h2><p><i class="fas fa-tint" style="color:#3366ff;"></i><span id="umi">0 %</span></p><div id="ctr" style="display:none;"><div id="btAto" style="display:none;"><span>Manual</span><button onclick="updateManual('off');">Automático</button></div><div id="btMan"><span>Automático</span><button onclick="updateManual('on');">Manual</button></div><br/><div id="flx" style="display:none;"><div id="btOn" style="display:none;"><span>Ligado</span><button onclick="updateAtivo('off');">Desligar</button></div><div id="btOf"><span>Desligado</span><button onclick="updateAtivo('on');">Ligar</button></div></div><div id="rle"><span>Ativa menor que</span><div><input onchange="updateTextInput(this.value);" type="range" min="1" max="100" value="0" id="rng"/></div><span id="rngS">0 %</span><span style="margin-top:20px;">Tempo ligado (Segundos)</span><div><input onchange="changeTime(this.value);" type="number" id="timeOn"/></div></div></div></body><script src="https://code.jquery.com/jquery-3.5.1.slim.min.js"></script><script src="https://ajax.googleapis.com/ajax/libs/jquery/3.5.1/jquery.min.js"></script><script>var requesting=false;function requestUrl(method,url) {requesting=true;$.ajax({url:url,method:method,success:()=>{requesting=false;},error:()=>{requesting=false;}});}function updateTextInput(val) {$("#rngS").text(`${val}%`);requestUrl("POST",`${window.location.href}regra?valor=${val}`);}function changeTime(val) {$("#rngS").val(val);requestUrl("POST",`${window.location.href}tempo?valor=${val}`);}function btnModo(val) {if (val == "off") {$("#btMan").show();$("#btAto").hide();$("#flx").hide();$("#rle").show();}else {$("#btMan").hide();$("#btAto").show();$("#flx").show();$("#rle").hide();}}function btOnOff(val) {if (val == "on") {$("#btOn").show();$("#btOf").hide();}else {$("#btOn").hide();$("#btOf").show();}}function updateManual(val) {requestUrl("POST",`${window.location.href}manual/${val}`);btnModo(val);}function updateAtivo(val) {requestUrl("POST",`${window.location.href}ativo/${val}`);btOnOff(val);}function sta(){$.ajax({url:`${window.location.href}controle`,method:"GET",success:(response)=>{if(!!response){const controle=response.split(";");$("#umi").text(`${controle[0]}%`);$("#rng").val(parseInt(controle[1]));$("#rngS").text(`${controle[1]}%`);$("#timeOn").val(parseInt(controle[4]));btnModo(controle[2]=="1"?"on":"off");btOnOff(controle[3]=="1"?"on":"off");}$("#ctr").show();}});}setInterval(()=>{if(!requesting)sta();},10000);sta();</script></html>)rawliteral";
    Server.send(200, "text/html", content);
}

void ObterControle()
{
    String retorno = "";
    retorno += valorSensor;
    retorno += ";";
    retorno += umidadeAutomatico;
    retorno += ";";
    retorno += manual;
    retorno += ";";
    retorno += ativo;
    retorno += ";";
    retorno += tempoParaFicarLigado;
    Server.send(200, "text/plain", retorno);
}

void ManualOn()
{
    manual = true;
    SalvarManual();
    Server.send(200, "text/plain", String(""));
}

void ManualOff()
{
    manual = false;
    SalvarManual();
    Server.send(200, "text/plain", String(""));
}

void AtivoOn()
{
    ativo = true;
    AtivarSelenoide();
    Server.send(200, "text/plain", String(""));
    EnviarAtivo();
}

void AtivoOff()
{
    ativo = false;
    DesativarSelenoide();
    Server.send(200, "text/plain", String(""));
    EnviarDesativado();
}

void Regra()
{
    umidadeAutomatico = atoi(Server.arg(0).c_str());
    Serial.print("Valor automático alterado para:");
    Serial.println(Server.arg(0));
    SalvarUmidade();
    Server.send(200, "text/plain", String(""));
}

void Tempo()
{
    tempoParaFicarLigado = atoi(Server.arg(0).c_str());
    Serial.print("Tempo alterado para:");
    Serial.println(Server.arg(0));
    SalvarTempo();
    Server.send(200, "text/plain", String(""));
}

void ConfiguraWifi()
{
    Serial.println("Configurando o Wifi");
    Portal.config(Config);
    if (Portal.begin())
    {
        Serial.println("IP encontrado: " + WiFi.localIP().toString());
    }

    Server.on("/", HTTP_GET, PaginaInicial);
    Server.on("/controle", HTTP_GET, ObterControle);
    Server.on("/manual/on", HTTP_POST, ManualOn);
    Server.on("/manual/off", HTTP_POST, ManualOff);
    Server.on("/ativo/on", HTTP_POST, AtivoOn);
    Server.on("/ativo/off", HTTP_POST, AtivoOff);
    Server.on("/regra", HTTP_POST, Regra);
    Server.on("/tempo", HTTP_POST, Tempo);
    Server.begin();
}

void IniciarFS()
{
    if (!SPIFFS.begin())
    {
        Serial.println("\nErro ao abrir o sistema de arquivos");
    }
    else
    {
        Serial.println("\nSistema de arquivos aberto com sucesso!");
    }
}

void LerConfiguracao()
{
    String configuracaoManual = LerArquivo("/manual.txt");
    if (!configuracaoManual.isEmpty())
    {
        manual = configuracaoManual == "1";
    }
    String tempoFicarLigado = LerArquivo("/tempoFicarLigado.txt");
    if (!tempoFicarLigado.isEmpty())
    {
        tempoParaFicarLigado = atoi(tempoFicarLigado.c_str());
    }
    String umidade = LerArquivo("/umidade.txt");
    if (!umidade.isEmpty())
    {
        umidadeAutomatico = atoi(umidade.c_str());
    }
}

void IniciaMaquinaSiot()
{
    EnviarHttp("/state/machine", "{\"idMachine\": \"1313\",\"operation\": \"Analisar umidade do solo\",\"status\": \"active\",\"date\": \"2020-05-29T03:18:20.000Z\",\"message\": \"\"}");
}

void setup()
{
    Serial.begin(115200);
    ConfiguraPinoReleEIniciaDesligado();
    ConfigurandoPinoSensorSolo();
    IniciarFS();
    LerConfiguracao();
    delay(1000);
    ConfiguraWifi();
    delay(1000);
    IniciaMaquinaSiot();
}

void loop()
{
    Portal.handleClient();
    LerSensorSolo();
    delay(1000);
    delaySendSiot++;
    if (delaySendSiot < 60)
    {
        EnviarHttp("/state/sensor", "{\"idMachine\": \"1313\",\"status\": \"active\",\"date\": \"2020-05-29T12:05:20.000Z\",\"message\": \"\",\"sensorId\": \"soilmoisture\",\"sensor\": \"\",\"signals\": [{\"signal\": \"soilmoisture\",\"value\":" + String(valorSensor) + " }]}");
    }
}