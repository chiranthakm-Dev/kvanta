#include <gtest/gtest.h>
#include "rcu_store.h"

TEST(RcuStoreTest, GetSet) {
    RcuStore<std::string, std::string> store;
    store.set("key", "value");
    auto val = store.get("key");
    ASSERT_TRUE(val);
    ASSERT_EQ(*val, "value");
}

TEST(RcuStoreTest, Del) {
    RcuStore<std::string, std::string> store;
    store.set("key", "value");
    auto val = store.del("key");
    ASSERT_TRUE(val);
    ASSERT_EQ(*val, "value");
    val = store.get("key");
    ASSERT_FALSE(val);
}
