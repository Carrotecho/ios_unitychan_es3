ios_unitychan_es3
=================

OpenGL ES 3.0でユニティちゃんのゲームを作ってみるプロジェクト

* 動作環境
 - Xcode 5.1.1
 - iOS SDK 7.1
 - iPhone5sなどOpenGL ES 3.0に対応した端末
 - 64bit版のiPhoneシミュレータには未対応

* 外部依存ライブラリ
 - FBX SDK 2015.1 iOS

/ApplicationsディレクトリにFBX SDKをインストール後  
/Applications/Autodesk/FBX SDK/2015.1/libへ移動し以下のコマンドでライブラリのユニバーサル化が必要  
lipo -create ./ios-armv7/release/libfbxsdk.a ./ios-i386/release/libfbxsdk.a -output libfbxsdk.a

---

###ライセンス

<div><img src="http://unity-chan.com/images/imageLicenseLogo.png" alt="ユニティちゃんライセンス"><p>このアセットは、『<a href="http://unity-chan.com/download/license.html" target="_blank">ユニティちゃんライセンス</a>』で提供されています。このアセットをご利用される場合は、『<a href="http://unity-chan.com/download/guideline.html" target="_blank">キャラクター利用のガイドライン</a>』も併せてご確認ください。</p></div>


Lisenceディレクトリ以下にライセンスファイルを含めていますので合わせてご覧ください。  
何か問題があれば@ramemisoまで
