#
# カスタムプラグイン設定
# krkrz_dev 以下のプラグインの他任意フォルダで krkrz_dev と
# 同じ構造で作成できるプラグインを直接取り込めます

# プラグインフォルダ追加
LIST(APPEND TVP_PLUGIN_FOLDERS
)

# プラグイン追加
LIST(APPEND TVP_PLUGINS
  csvParser
)
