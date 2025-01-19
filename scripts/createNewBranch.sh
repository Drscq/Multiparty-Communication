# Check the current status
git status

# Create a new branch
git checkout -b feature/party-communication

# Add all changes to the staging area
git add .

# Commit the changes
git commit -m "Add Party communication and sum calculation"

# Push the new branch to the remote repository
git push origin feature/party-communication

# Verify the branch has been pushed
git branch -r