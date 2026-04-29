#
# カスタムプラグイン設定
# krkrz_dev 以下のプラグインの他任意フォルダで krkrz_dev と
# 同じ構造で作成できるプラグインを直接取り込めます

# プラグインフォルダ追加
# app/src/main/cpp 配下にも Android 専用プラグイン (Java連携) を置く
LIST(APPEND TVP_PLUGIN_FOLDERS
  ${CMAKE_CURRENT_LIST_DIR}/app/src/main/cpp
)

# プラグイン追加
LIST(APPEND TVP_PLUGINS
  csvParser
  playgames
)

# 静的リンク (libkrkrz.so に組み込み)
# Android では .so 単体ロードのほうが扱いやすいので playgames は静的化する
LIST(APPEND TVP_PLUGINS_STATIC
  playgames
)
