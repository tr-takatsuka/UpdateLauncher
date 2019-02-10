
// ATL 及び boost で必要なインクルードをひとまとめにした物
// このファイルを各プロジェクトの stdafx.h でインクルードするがよろし

#pragma once

#ifndef __linux__

	// TODO: プログラムに必要な追加ヘッダーをここで参照してください。

	//#if _MSC_VER < 1600			// VisualStudio2010未満なら
	//	#define _HAS_ITERATOR_DEBUGGING 0		// デバッグ時の STL の範囲チェックは不要
	//#endif

	#include <typeinfo.h>

	#define _USE_MATH_DEFINES
	#include <math.h>



	#include <afx.h>
	#include <afxwin.h>         // MFC のコアおよび標準コンポーネント
	#include <afxext.h>         // MFC の拡張部分

	#include <atlbase.h>
	#include <atlcom.h>
	//#include <atlstr.h>
	#include <atlutil.h>
	#include <atlfile.h>
	#include <atlimage.h>
	#include <atlwin.h>
	#include <atlsecurity.h>

	#include <mmsystem.h>

#endif	//#ifndef __linux__

#include <algorithm>
#include <codecvt>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <filesystem> 
#include <fstream> 
//#include <hash_set>			VisualStudio2015ではNG?
//#include <hash_map>			VisualStudio2015ではNG?
#include <iostream>
#include <list>
#include <limits>
#include <map>
#include <mutex>
#include <regex>
#include <set>
// #include <strstream>			C++17 非推奨
#include <stdarg.h>
#include <stdexcept>
#include <typeinfo>
#include <vector>


#ifdef _DEBUG
	#define BOOST_ENABLE_ASSERT_HANDLER
#else
	#define BOOST_DISABLE_ASSERTS
#endif

#define BOOST_ASIO_DISABLE_IOCP
//#define BOOST_ASIO_ENABLE_CANCELIO
//#define BOOST_ASIO_DISABLE_STD_CHRONO

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/hex.hpp>
#include <boost/any.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/insert_linebreaks.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/spawn.hpp>
//#include <boost/bind.hpp>
//#include <boost/chrono/chrono.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/cstdint.hpp>
#include <boost/crc.hpp>
#include <boost/dll/runtime_symbol_info.hpp>
//#include <boost/enable_shared_from_this.hpp>
#include <boost/filesystem.hpp>
//#include <boost/function.hpp>
#include <boost/interprocess/windows_shared_memory.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/stream.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/locale/generator.hpp>
#include <boost/locale/encoding.hpp> 
#include <boost/locale/util.hpp> 
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/debug_output_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/trivial.hpp>
#include <boost/noncopyable.hpp>
//#include <boost/math/special_functions/round.hpp>
//#include <boost/move/unique_ptr.hpp>
#include <boost/multi_array.hpp>
#include <boost/optional.hpp>
#include <boost/program_options.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/range.hpp>
#include <boost/regex.hpp>
//#include <boost/shared_ptr.hpp>
//#include <boost/thread.hpp>
//#include <boost/thread/condition.hpp>
//#include <boost/tuple/tuple.hpp>
#include <boost/uuid/name_generator_md5.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/version.hpp>
//#include <boost/weak_ptr.hpp>
