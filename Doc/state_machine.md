# RobotC 状態遷移図・運転モード設計

## 概要

本ドキュメントは、RobotCの「通常モード（PC連携）」と「縮小運転モード（PC未接続）」を含む状態遷移設計をまとめたものです。

---

## 運転モードと状態遷移図

```mermaid
stateDiagram-v2
    [*] --> MQTT_Connected : 起動/再接続
    MQTT_Connected : 通常モード\n(PC連携)
    MQTT_Connected --> MQTT_Lost : MQTTタイムアウト
    MQTT_Lost : 縮小運転モード\n(ローカル自律)
    MQTT_Lost --> MQTT_Connected : MQTT再接続

    state MQTT_Connected {
        [*] --> Idle
        Idle --> PersonDetected : 人検出
        PersonDetected --> Approach : 距離>しきい値
        PersonDetected --> Stop : 距離<=しきい値
        Approach --> Stop : 近づいた
        Stop --> Conversation : 声をかけられる
        Conversation --> Idle : 会話終了
        Idle --> Wander : 人がいない
        Wander --> Idle : 人検出
    }

    state MQTT_Lost {
        [*] --> LocalIdle
        LocalIdle --> LocalPersonDetected : 人検出
        LocalPersonDetected --> LocalApproach : 距離>しきい値
        LocalPersonDetected --> LocalStop : 距離<=しきい値
        LocalApproach --> LocalStop : 近づいた
        LocalStop --> LocalReply : 声をかけられる
        LocalReply --> LocalIdle : 相槌終了
        LocalIdle --> LocalWander : 人がいない
        LocalWander --> LocalIdle : 人検出
    }
```

---

## 各状態の説明

### 通常モード（MQTT_Connected）
- **Idle**: 待機状態。人検出を監視
- **PersonDetected**: 人を検出した直後
- **Approach**: 距離が遠いので接近
- **Stop**: 目標距離で停止
- **Conversation**: 会話・感情表現
- **Wander**: 人がいない場合の徘徊

### 縮小運転モード（MQTT_Lost）
- **LocalIdle**: 待機状態。人検出を監視
- **LocalPersonDetected**: 人を検出した直後
- **LocalApproach**: 距離が遠いので接近
- **LocalStop**: 目標距離で停止
- **LocalReply**: 声をかけられたら簡単な相槌
- **LocalWander**: 人がいない場合の徘徊

---

## モード切り替え仕様
- MQTT接続の有無を定期監視
- タイムアウト時は自動で縮小運転モードへ
- 再接続時は通常モードに復帰

---

## 今後の拡張案
- 状態ごとの詳細なサブステート設計
- エラー・異常時のフェイルセーフ遷移
- 各状態でのLED・音声・動作パターン定義
