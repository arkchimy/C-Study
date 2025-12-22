1. cpp_redis 소스코드 다운로드

https://github.com/cpp-redis/cpp_redis
Code > Download Zip 으로 최신 cpp redis  버전을 다운로드 받음

2. tacopie 소스코드 다운로드
https://github.com/cpp-redis/tacopie
cpp-redis 가 tacopie tcp 라이브러리를 기반으로 만들어져서 tacopie 소스를 또 받아야함.

3. tacopie 코드를 cpp-redis 폴더로 이동
cpp-redis 의 tacopie 폴더에 가보면  아무것도 없음이 확인됨.
cpp_redis-master\tacopie

여기에 tacopie-master.zip 의 압축을 풀어서 그대로 집어넣음.

4. cpp-redis 빌드
cpp_redis-master\msvc15  폴더에서  cpp_redis.sln 을 비주얼스튜디오로 오픈 후 빌드

cpp_redis 와 tacopie 2개의 프로젝트가 모두 잘 빌드 되기를 기도 합니다

* 확인필요사항

cpp_redis 프로젝트 속성 > 일반 > Windows SDK / 플렛폼 도구집합
tacopie 프로젝트 속성 > 일반 > Windows SDK / 플렛폼 도구집합

위 2 항목에 대해 내 Visual Studio 에 설치된 항목으로 선택


공용 포함 아님 ( 매우 중요  )
프로젝트 속성 > VC++디렉터리 > 포함 디렉터리에       $(ProjectDir);      추가



cpp_redis-master\msvc15\x64\Debug   폴더의   (빌드 상황에 따라 적절한 폴더 나오겠음)

cpp_redis.lib
cpp_redis.pdb
tacopie.lib
tacopie.pdb

