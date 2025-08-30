#include <gtest/gtest.h>
#include "astrocomm/server/repositories/user_repository.h"
#include <memory>
#include <filesystem>

using namespace astrocomm::server::repositories;

class UserRepositoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDataPath_ = "./test_data/users_test.json";
        std::filesystem::create_directories("./test_data");
        repository_ = UserRepositoryFactory::createRepository(testDataPath_);
        ASSERT_NE(repository_, nullptr);
    }
    
    void TearDown() override {
        if (std::filesystem::exists(testDataPath_)) {
            std::filesystem::remove(testDataPath_);
        }
    }
    
    services::UserInfo createTestUser(const std::string& id) {
        services::UserInfo user;
        user.userId = id;
        user.username = "testuser_" + id;
        user.email = "test_" + id + "@example.com";
        user.firstName = "Test";
        user.lastName = "User " + id;
        user.passwordHash = "hashed_password_" + id;
        user.roles = {"user"};
        user.permissions = {"read", "write"};
        user.isActive = true;
        return user;
    }
    
    std::unique_ptr<IUserRepository> repository_;
    std::string testDataPath_;
};

TEST_F(UserRepositoryTest, BasicCRUDOperations) {
    auto user = createTestUser("1");
    
    // Create
    EXPECT_TRUE(repository_->create(user));
    EXPECT_TRUE(repository_->exists(user.userId));
    EXPECT_EQ(repository_->count(), 1);
    
    // Read
    auto retrieved = repository_->read(user.userId);
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->userId, user.userId);
    EXPECT_EQ(retrieved->username, user.username);
    
    // Update
    user.firstName = "Updated";
    EXPECT_TRUE(repository_->update(user));
    
    auto updated = repository_->read(user.userId);
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated->firstName, "Updated");
    
    // Delete
    EXPECT_TRUE(repository_->remove(user.userId));
    EXPECT_FALSE(repository_->exists(user.userId));
    EXPECT_EQ(repository_->count(), 0);
}

TEST_F(UserRepositoryTest, AuthenticationOperations) {
    auto user = createTestUser("auth_test");
    repository_->create(user);
    
    // Find by username
    auto found = repository_->findByUsername(user.username);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->userId, user.userId);
    
    // Find by email
    auto foundByEmail = repository_->findByEmail(user.email);
    ASSERT_TRUE(foundByEmail.has_value());
    EXPECT_EQ(foundByEmail->userId, user.userId);
    
    // Validate credentials
    EXPECT_TRUE(repository_->validateCredentials(user.username, user.passwordHash));
    EXPECT_FALSE(repository_->validateCredentials(user.username, "wrong_password"));
    
    // Update password
    std::string newPassword = "new_hashed_password";
    EXPECT_TRUE(repository_->updatePassword(user.userId, newPassword));
    EXPECT_TRUE(repository_->validateCredentials(user.username, newPassword));
    EXPECT_FALSE(repository_->validateCredentials(user.username, user.passwordHash));
}

TEST_F(UserRepositoryTest, RoleManagement) {
    auto user = createTestUser("role_test");
    repository_->create(user);
    
    // Add role
    EXPECT_TRUE(repository_->addRole(user.userId, "admin"));
    EXPECT_TRUE(repository_->hasRole(user.userId, "admin"));
    EXPECT_TRUE(repository_->hasRole(user.userId, "user")); // Original role
    
    // Get users by role
    auto adminUsers = repository_->getUsersByRole("admin");
    EXPECT_EQ(adminUsers.size(), 1);
    EXPECT_EQ(adminUsers[0].userId, user.userId);
    
    // Remove role
    EXPECT_TRUE(repository_->removeRole(user.userId, "user"));
    EXPECT_FALSE(repository_->hasRole(user.userId, "user"));
    EXPECT_TRUE(repository_->hasRole(user.userId, "admin"));
}

TEST_F(UserRepositoryTest, PermissionManagement) {
    auto user = createTestUser("perm_test");
    repository_->create(user);
    
    // Add permission
    EXPECT_TRUE(repository_->addPermission(user.userId, "admin"));
    EXPECT_TRUE(repository_->hasPermission(user.userId, "admin"));
    EXPECT_TRUE(repository_->hasPermission(user.userId, "read")); // Original permission
    
    // Remove permission
    EXPECT_TRUE(repository_->removePermission(user.userId, "read"));
    EXPECT_FALSE(repository_->hasPermission(user.userId, "read"));
    EXPECT_TRUE(repository_->hasPermission(user.userId, "admin"));
}

TEST_F(UserRepositoryTest, SearchOperations) {
    auto user1 = createTestUser("search1");
    user1.firstName = "John";
    user1.lastName = "Doe";
    
    auto user2 = createTestUser("search2");
    user2.firstName = "Jane";
    user2.lastName = "Smith";
    
    repository_->create(user1);
    repository_->create(user2);
    
    // Search by name
    auto johnResults = repository_->search("John");
    EXPECT_EQ(johnResults.size(), 1);
    EXPECT_EQ(johnResults[0].userId, user1.userId);
    
    // Search by email
    auto emailResults = repository_->search("search1@example.com");
    EXPECT_EQ(emailResults.size(), 1);
    EXPECT_EQ(emailResults[0].userId, user1.userId);
}

TEST_F(UserRepositoryTest, InvalidOperations) {
    // Try to create user with duplicate username
    auto user1 = createTestUser("1");
    auto user2 = createTestUser("2");
    user2.username = user1.username; // Same username
    
    EXPECT_TRUE(repository_->create(user1));
    EXPECT_FALSE(repository_->create(user2)); // Should fail
    
    // Try to create user with duplicate email
    auto user3 = createTestUser("3");
    user3.email = user1.email; // Same email
    EXPECT_FALSE(repository_->create(user3)); // Should fail
}
