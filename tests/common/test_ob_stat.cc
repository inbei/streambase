#include "ob_statistics.h"
#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;
using namespace sb;
using namespace sb::common;

TEST(ObStat, Serialize) {
  ObStat stat;
  stat.set_table_id(1111);
  stat.set_value(3, 20);
  stat.inc(3);
  char buff[1024];
  int64_t pos = 0;
  EXPECT_EQ(1111, stat.get_table_id());
  EXPECT_EQ(21, stat.get_value(3));
  EXPECT_TRUE(OB_SUCCESS == stat.serialize(buff, 1024, pos));
  pos = 0;
  ObStat stat2;
  EXPECT_TRUE(OB_SUCCESS == stat2.deserialize(buff, 1024, pos));
  EXPECT_EQ(1111, stat2.get_table_id());
  EXPECT_EQ(21, stat2.get_value(3));

}
TEST(ObStatManager, Serialize) {
  ObStatManager stat_manager(ObStatManager::SERVER_TYPE_CHUNK);
  stat_manager.set_value(1001, 1, 1);
  stat_manager.set_value(1002, 2, 2);
  stat_manager.set_value(1001, 2, 2);
  stat_manager.inc(1002, 2);
  stat_manager.inc(1001, 3, 5);
  stat_manager.inc(1001, 3, 5);

  ObStatManager::const_iterator it = stat_manager.begin();
  EXPECT_EQ(1001, it->get_table_id());
  EXPECT_EQ(1, it->get_value(1));
  EXPECT_EQ(2, it->get_value(2));
  EXPECT_EQ(10, it->get_value(3));
  it++;
  EXPECT_EQ(1002, it->get_table_id());
  EXPECT_EQ(3, it->get_value(2));
  EXPECT_EQ(0, it->get_value(1));
  it++;
  EXPECT_EQ(stat_manager.end(), it);

  char buff[1024];
  int64_t pos = 0;
  EXPECT_TRUE(OB_SUCCESS == stat_manager.serialize(buff, 1024, pos));
  pos = 0;
  ObStatManager m2(ObStatManager::SERVER_TYPE_MERGE);
  EXPECT_TRUE(OB_SUCCESS == m2.deserialize(buff, 1024, pos));
  EXPECT_EQ(ObStatManager::SERVER_TYPE_CHUNK, m2.get_server_type());

  it = m2.begin();
  EXPECT_EQ(1001, it->get_table_id());
  EXPECT_EQ(1, it->get_value(1));
  EXPECT_EQ(2, it->get_value(2));
  EXPECT_EQ(10, it->get_value(3));
  it++;
  EXPECT_EQ(1002, it->get_table_id());
  EXPECT_EQ(3, it->get_value(2));
  EXPECT_EQ(0, it->get_value(1));
  it++;
  EXPECT_EQ(m2.end(), it);


}
int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
