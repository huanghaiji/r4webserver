#include <SPI.h>
#include "LCD_Driver.h"
#include "GUI_Paint.h"
#include "qrcode.h"
#include "SdFat.h"
#include "FreeStack.h"
#include "WiFiS3.h"
#include "URLCode.h"
#include <Arduino_LED_Matrix.h>
#include <Adafruit_AHTX0.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HX711.h>

const int chipSelect = 5;
SdFat sd;
SdFile file;

int status = WL_IDLE_STATUS;
int runnum = 0;
long lcdtime = -100000000;
long lcdtimemax = 5000;
WiFiServer server(80);
bool isip = false;
bool serveronbegin = false;

uint8_t frame[8][12] = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
uint8_t point = 0;
long pointtim = 0;
ArduinoLEDMatrix matrix;
// Define URL Object
URLCode url;
Adafruit_AHTX0 aht;
float min_temperature = 10000;
float min_temperature_old24 = 0;
float humidity = 0;
float temperature = 0;
long oday = 0;
long sday = 0;
long uday = 0;  //提醒喂食时间更新记录

WiFiUDP ntpUDP;
unsigned long timeClientLockTim = 60000;
long timeClientLockMaxTim = 5 * timeClientLockTim;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 8 * 3600, timeClientLockTim);  // 设定NTP服务器，同步间隔（60秒）
int datemillis = millis();
long epochTime = -1;    //世界时间，单位为秒；
long postmessTime = 0;  //微信推送计时
long tempTime = 0;      //温度计时

HX711 scale;

void setup() {
  char ssid[] = "appdoor3";
  char pass[] = "hhj123456";

  matrix.begin();
  matrix_9(frame);

  sd.end();
  delay(1000);

  Config_Init();
  LCD_Init();

  LCD_SetBacklight(35);
  Paint_NewImage(LCD_WIDTH, LCD_HEIGHT, ROTATE_270, WHITE);
  Paint_Clear(WHITE);
  Serial.begin(9600);

  delay(2000);
  checkSD();

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Paint_DrawString_EN(0, 0, "Communication with WiFi module failed!", &Font20, RED, YELLOW);
    while (1)
      ;
  }
  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Paint_DrawString_EN(0, 0, (String("connect network,") + (millis() / 1000)).c_str(), &Font20, RED, YELLOW);
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(3000);
  }

  aht10step();
  timeClient.begin();
  hx711Step();
}

void loop() {
  while (true) {
    hx711();
    mainUI();
    serverpoll();
  }
}

void mainUI() {
  if ((millis() - lcdtime) > lcdtimemax) {
    runnum++;
    if (runnum > 10000) {
      runnum = 0;
    }
    long day = vm_date_util() / 86400;
    if (abs(day - oday) >= 1) {
      min_temperature_old24 = min_temperature;
      min_temperature = 10000;
      oday = day;
    }
    lcdtime = millis();
    //显示ip地址
    //显示wifi地址
    // Paint_Clear(WHITE);
    //左侧状态栏
    Paint_DrawRectangle(0, 0, 110, 21, GRAY, DOT_PIXEL_DFT, DRAW_FILL_FULL);
    Paint_DrawString_EN(0, 4, (String(" ") + runnum).c_str(), &Font16, GRAY, RED);
    //
    Paint_DrawString_EN(110, 0, WiFi.localIP().toString().c_str(), &Font20, RED, YELLOW);
    Paint_DrawString_EN(110, 20, (String("") + WiFi.SSID() + "(" + WiFi.RSSI() + ")").c_str(), &Font20, WHITE, 0xffcccccc);

    matrix_1(frame);
    qrcode();
    matrix_2(frame);
    aht10();
    matrix_3(frame);
    networkDate();
    matrix_4(frame);
    postmess();
    matrix_clear();
  }
}

int checkSD() {
  // check sd
  if (!sd.begin(chipSelect, SD_SCK_MHZ(16))) {
    Paint_DrawString_EN(110, 100, "SD ERROR", &Font20, WHITE, BLACK);
    return 0;
  }
  Paint_DrawString_EN(110, 100, "           ", &Font20, WHITE, BLACK);
  //
  loadSeatCof();
  return 1;
}

//二维码
void qrcode() {
  //二维码
  if (!isip) {
    isip = true;
    QRcode qrcode;
    qrcode.init();
    qrcode.create((String("http://") + WiFi.localIP().toString() + String(":80/application/index.html")).c_str(), 5, 12);
  }
}

//aht10温度计/湿度计
void aht10step() {
  if (!aht.begin()) {
    Serial.println("Init AHT10 Failure.");
  }
}

//10分钟（600）秒存储温/湿度;
void aht10() {
  sensors_event_t h, temp;
  aht.getEvent(&h, &temp);
  humidity = h.relative_humidity;
  temperature = temp.temperature;
  min_temperature = min(min_temperature, temperature);
  Paint_DrawString_EN(110, 40, (String("%RH:") + humidity + "%").c_str(), &Font20, YELLOW, RED);
  Paint_DrawString_EN(110, 60, (String("|C :") + temperature).c_str(), &Font20, YELLOW, RED);
  Paint_DrawString_EN(110, 80, (String("|mn:") + min_temperature + "," + min_temperature_old24).c_str(), &Font20, YELLOW, RED);
  long time = vm_date_util();
  if (time - tempTime > 600) {
    tempTime = time;
    long day = time / 86400;
    String path = String("") + day + ".time";
    file.open(path.c_str(), O_CREAT | O_APPEND | O_WRITE);
    file.println((String("") + time + "," + humidity + "," + temperature).c_str());
    file.close();
    Serial.println((String("写入一条数据") + path).c_str());
  }
}

//网站server
void serverpoll() {
  resetlcdOntim();
  if (!serveronbegin) {
    serveronbegin = true;
    server.begin();
  }
  // server.begin();
  WiFiClient client = server.available();  // listen for incoming clients
  if (client) {
    Serial.println("new client");
    String currentLine = "";
    uint8_t buf[1024];
    size_t c;
    while (client.connected()) {
      if (client.available()) {
        c = client.read(buf, 1024);
        currentLine += String(buf, c);
      }
      if (currentLine.endsWith("\r\n\r\n")) {
        Serial.println(currentLine);
        int si = currentLine.indexOf("GET /") + 5;
        int ei = currentLine.indexOf(" ", si);
        int mi = currentLine.indexOf("?", si);
        String path = currentLine.substring(si, mi == -1 ? ei : mi);
        url.urlcode = path;
        url.urldecode();
        path = url.strcode;
        url.release();
        Serial.println("tf请求路径:" + path + " \|");
        Paint_DrawString_EN(0, 154, "run web", &Font16, RED, YELLOW);
        if (file.open(path.c_str(), O_RDONLY)) {
          const int bufsize = 10240;
          uint8_t buf[bufsize];
          unsigned long fsize = file.size();
          unsigned long fnum = 0;
          Serial.print("文件长度:");
          Serial.println(fsize);
          if (path.endsWith(".js")) {
            client.println("HTTP/1.1 200");
            client.println("Content-Type: application/x-javascript");
            client.println();
          }
          while ((c = file.read(buf, bufsize)) > 0) {
            fnum += c;
            client.write(buf, c);
            processlcd(fnum * 1.0 / fsize);
          }
          file.close();
        } else if (path == "resetsd") {
          client.write((checkSD() == 0 ? "SD ERROR" : "SD SUCCESS"));
        } else if (!checkSD()) {
          client.write("<a href=\"/resetsd\">SD Error </a>");
        } else if (path == "resetdate") {  //更新世界时间
          timeClient.end();
          timeClient.begin();
          client.write("ok");
          client.write(vm_date_util());
        }
        Paint_DrawString_EN(0, 154, "end web", &Font16, BLACK, YELLOW);
        break;
      }
    }
    // close the connection:
    client.stop();
    Serial.println("client disconnected");
  }

  delay(64);
}

//中国天气气象局
void weaterNow(WiFiClient phone) {
  Serial.println("获取气象数据中");
  WiFiClient client;
  char host[] = "weather.cma.cn";
  if (client.connect(host, 80)) {
    client.println("GET /api/now/59287 HTTP/1.1");
    client.print("Host:");
    client.println(host);
    client.println("Connection: close");
    client.println();
  } else {
    Serial.println("<<<<<<-连接失败->>>>>");
    client.stop();
    return;
  }
  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 50000) {
      Serial.println("客户端超时");
      client.stop();
      break;
    }
  }
  while (client.available()) {
    String line = client.readStringUntil('\r');
    Serial.println("数据回应:" + line);
    phone.println(line);
  }

  Serial.println();
  Serial.println("关闭链接");
  client.stop();
}

//同步世界时间
void networkDate() {
  if (millis() - datemillis > timeClientLockMaxTim) {
    timeClient.end();
    timeClient.begin();
    Serial.println((String("同步世界时间失败") + vm_date_util()).c_str());
  }
  if (timeClient.update()) {
    datemillis = millis();
    epochTime = timeClient.getEpochTime();
    Serial.print("世界同步: 周");
    Serial.print(timeClient.getDay());
    Serial.print(",时间戳(秒)");
    Serial.println(timeClient.getEpochTime());
    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    int d = timeClient.getDay();
    Paint_DrawString_EN(10, 120, (String("Day:") + d + "  Time:" + (h > 9 ? "" : "0") + h + ":" + (m > 9 ? "" : "0") + m).c_str(), &Font20, WHITE, BLACK);
  }
}

//虚拟当前时间戳
long vm_date_util() {
  if (epochTime == -1)
    return epochTime;
  return epochTime + (millis() - datemillis) / 1000;
}

//推送消息到微信,间隔600秒（10分钟）
void postmess() {
  if ((vm_date_util() - postmessTime) >21600) {
    String content = String("") + "温度:" + temperature + "℃"
                     + ",湿度:" + humidity + "%"
                     + ",最低温度:" + min_temperature + "℃"
                     + ",24小时前最低温度:" + min_temperature_old24 + "℃";
    if (wechatPostmess(content)) {
      postmessTime = vm_date_util();
    }
  }
  long day = vm_date_util() / 86400;
  if (day - sday > 2) {
    if (uday - day >= 1) {
      if (wechatPostmess(String("喂食超过") + (day - sday) + " 天")) {
        uday = day;
      }
    }
  }
}

bool wechatPostmess(String content) {
  bool isok = false;
  Paint_DrawString_EN(0, LCD_WIDTH - 21, "VX", &Font20, WHITE, BLACK);
  WiFiSSLClient client;
  String datastr = String("{\"appToken\":\"AT_GmozRH3lVdUaKYqopFEQAfS7vTH1WW2J\",\"content\":\"")
                   + content
                   + "\",\"contentType\":2,\"uids\":[\"UID_5Lqa459vXsGseDZPx58Evi2sS1K7\"],\"verifyPayType\":0}";
  const char* data = datastr.c_str();
  int len = strlen(data);
  Serial.println(data);
  Serial.println(len);
  if (client.connect("wxpusher.zjiecode.com", 443)) {
    Serial.println("推送消息POST");
    client.println("POST /api/send/message HTTP/1.1");
    client.println("Host: wxpusher.zjiecode.com");
    client.println("Content-Type: application/json");
    client.println((String("Content-Length: ") + len).c_str());
    client.println("Connection: close");
    client.println();
    client.println(data);
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 50000) {
        Serial.println("客户端超时");
        break;
      }
    }
    while (client.available()) {
      Serial.print((char)client.read());
    }
    isok = true;
  }
  client.stop();
  Paint_DrawString_EN(0, LCD_WIDTH - 21, "  ", &Font20, WHITE, BLACK);
  return isok;
}

//称重
void hx711Step() {
  scale.begin(2, 4);
  scale.set_scale((249340 - 245271) / 10.00);
  scale.tare();
}

void hx711() {
 // matrix_5(frame);
  if (checkHx711JoinHome()) {

    Paint_Clear(WHITE);
    Paint_DrawRectangle(0, 0, LCD_HEIGHT, 60, YELLOW, DOT_PIXEL_DFT, DRAW_FILL_FULL);

    int selectindex = 0;
    int keycount = 0;
    const char* areas[2] = { "A1", "B2" };
    const char* area = areas[selectindex];
    int n = 4;  //sizeof(areas)/sizeof(areas[0]);
    while (true) {
      Paint_DrawString_EN(20, 70, (String("") + areas[0] + (selectindex == 0 && keycount != 0 ? (" to " + String(keycount * 100 / n) + "%") : "         ")).c_str(), &Font20, WHITE, BLACK);
      Paint_DrawString_EN(20, 100, (String("") + areas[1] + (selectindex == 1 && keycount != 0 ? (" to " + String(keycount * 100 / n) + "%") : "         ")).c_str(), &Font20, WHITE, BLACK);
      Paint_DrawString_EN(20, 20, areas[selectindex], &Font20, YELLOW, BLACK);
      if(weightOnKeyevent(250,50)){
        keycount++;
        continue;
      }
      if( 0 < keycount && keycount < n){
        selectindex++;
        if (selectindex >= 2) {
          selectindex = 0;
        }
      }else if( n <= keycount ){
        area = areas[selectindex];
        break;
      }
      keycount =0;
    }
    //去皮误差必须小于下个称重误差，以免造成直接跳过。
    Paint_Clear(WHITE);
    if( weightValue()<2 ){
      Paint_DrawString_EN(20,20,"preless peel",&Font20,WHITE,BLACK);
    }
    while( true){
      if(weightValue()<1){
        Paint_DrawString_EN(20,20,"on peel........",&Font20,WHITE,BLACK);
        scale.tare(250);
        break;
      }
    }

    Paint_Clear(WHITE);
    Paint_DrawRectangle(0, 0, LCD_HEIGHT, 21, YELLOW, DOT_PIXEL_DFT, DRAW_FILL_FULL);

    long update_ui_time = millis();
    float z;
    int z2 =0;
    bool lop=true;
    int  lopn=0;
    while(lop){
      z = weightValue();
      z2  = max(z2,z);
      Paint_DrawString_EN(50, 40, (String("weight:") + z).c_str(), &Font20, WHITE, BLACK);
      Paint_DrawString_EN(50, 70, (String("area:") + area).c_str(), &Font20, WHITE, RED);
      Paint_DrawString_EN(50, 90, (String("weight max:") + z2).c_str(), &Font20, WHITE, RED);
      if(2<z2 && z <=2){
        if(lopn==0){
          continue;
        }
        lop=false;
      }
      lopn++;
    }

    scale.tare(250);
    Paint_Clear(WHITE);
    Paint_DrawRectangle(0, 0, LCD_HEIGHT, 60, YELLOW, DOT_PIXEL_DFT, DRAW_FILL_FULL);
    n=4;
    keycount=0;
    selectindex=0;
    int maxselect=3;
    const char* formats[3] = { "mixedGrains", "Applet","WEIGHT ERROR!" };
    const char* format = formats[selectindex];
    while (true) {
      Paint_DrawString_EN(20, 20, (formats[selectindex]+String("          ")).c_str(), &Font20, YELLOW, BLACK);
      Paint_DrawString_EN(20, 70, (String("") + formats[0] + (selectindex == 0 && keycount != 0 ? (" to " + String(keycount * 100 / n) + "%") : "         ")).c_str(), &Font20, WHITE, BLACK);
      Paint_DrawString_EN(20, 100, (String("") + formats[1] + (selectindex == 1 && keycount != 0 ? (" to " + String(keycount * 100 / n) + "%") : "         ")).c_str(), &Font20, WHITE, BLACK);
      Paint_DrawString_EN(20, 120, (String("") + formats[2] + (selectindex == 2 && keycount != 0 ? (" to " + String(keycount * 100 / n) + "%") : "         ")).c_str(), &Font20, WHITE, BLACK);
      if(weightOnKeyevent(250,50)){
        keycount++;
        continue;
      }
      if( 0 < keycount && keycount < n){
        selectindex++;
        if (selectindex >= maxselect) {
          selectindex = 0;
        }
      }else if( n <= keycount ){
        format = formats[selectindex];
        break;
      }
      keycount =0;
    }

    //去皮后会出现负数
    if (z2 > 0) {
      //保存
      long time = vm_date_util();
      long day = time / 86400;
      String path = String("") + day + ".seat";
      file.open(path.c_str(), O_CREAT | O_APPEND | O_WRITE);
      file.print(area);
      file.print(",");
      file.print(z2);
      file.print(",");
      file.print(format);
      file.println();
      file.close();
      sday = day;
      uday = day;
      saveSeatCof();
      Serial.println((String("写入一条数据") + path).c_str());
    }
    Paint_Clear(WHITE);

    isip=false;
    scale.tare(250);
  }
  //matrix_clear();
}

//判断是否进入称重状态，则停止其他传感器事件;
int hxuistartnum= 0;
bool checkHx711JoinHome(){
 if (scale.wait_ready_retry(50)) {
    long reading = scale.read();
    int z = (int)scale.get_units();
    Paint_DrawString_EN(110, 150, (String("weight:") + z + "," + hxuistartnum).c_str(), &Font20, WHITE, RED);
    if (z > 100) {
      hxuistartnum++;
    } else {
      hxuistartnum = 0;
    }
  }
  bool opt = hxuistartnum>1;
  if(opt){
     hxuistartnum= 0;
  }
  return opt;
}
bool weightOnKeyevent(int delayms,int weightKeyValue){
  if(scale.wait_ready_retry(delayms)){
    int z = scale.get_units();
    Serial.println(z);
    if(z >weightKeyValue){
      return true;
    }
  }
  return false;
}
float weightValue(){
  scale.wait_ready_retry(250);
  return scale.get_units();
}


void saveSeatCof() {
  file.open("seat.cof", O_CREAT | O_WRITE);
  file.println(sday);
  file.println(uday);
  file.close();
}

void loadSeatCof() {
  //设备启动时
  if (sday <= 0) {
    file.open("seat.cof", O_READ);
    sday = readLine().toInt();
    uday = readLine().toInt();
    file.close();
  } else {
    //运行中sd发生意外时。
    saveSeatCof();
  }
}

//读取一行字符粗数据
String readLine() {
  String str("");
  for (int ci; (ci = file.read()) != -1;) {
    char ch = (char)ci;
    Serial.println(ch);
    if (ch == '\r') {
      break;
    }
    if (ch != '\n') {
      str += ch;
    }
  }
  return str;
}

//全部清除lcd
void resetlcdOntim() {
  if (millis() - pointtim > 2000 && point >= 96) {
    pointtim = millis();
    point = 0;
    for (; point < 96; point++) {
      frame[point / 12][point % 12] = 0;
    }
    point = 0;
    matrix.renderBitmap(frame, 8, 12);
  }
}

//lcd显示传输上送进度
void processlcd(float p) {
  int max = (uint8_t)(96 * p);
  for (point = 0; point < max; point++) {
    frame[point / 12][point % 12] = 1;
  }
  for (; point < 96; point++) {
    frame[point / 12][point % 12] = 0;
  }
  matrix.renderBitmap(frame, 8, 12);
}

void matrix_clear(){
  matrix_clear(frame);
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_clear(uint8_t frame[8][12]){
  for(int x=0; x< 8; x++){
    for(int y=0; y<12; y++){
      frame[x][y] = 0;
    }
  }
}
//lcd显示1，以此类推
void matrix_1(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][5] = 1;
  frame[2][5] = 1;
  frame[3][5] = 1;
  frame[4][5] = 1;
  frame[5][5] = 1;
  frame[6][5] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_2(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][6] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][3] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_3(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][6] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][6] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_4(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][6] = 1;
  frame[2][3] = 1;
  frame[2][6] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][6] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_5(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][3] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][6] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_6(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][3] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][3] = 1;
  frame[4][6] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_7(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][6] = 1;
  frame[3][6] = 1;
  frame[4][6] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_8(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][3] = 1;
  frame[2][6] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][3] = 1;
  frame[4][6] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
void matrix_9(uint8_t frame[8][12]) {
  matrix_clear(frame);
  frame[1][3] = 1;
  frame[1][4] = 1;
  frame[1][5] = 1;
  frame[1][6] = 1;
  frame[2][3] = 1;
  frame[2][6] = 1;
  frame[3][3] = 1;
  frame[3][4] = 1;
  frame[3][5] = 1;
  frame[3][6] = 1;
  frame[4][6] = 1;
  frame[5][3] = 1;
  frame[5][4] = 1;
  frame[5][5] = 1;
  frame[5][6] = 1;
  matrix.renderBitmap(frame, 8, 12);
}
