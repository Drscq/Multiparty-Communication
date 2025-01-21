# Check the current status
git status

# Create a new branch
git checkout -b feature/reqrep-dealerrouter

# Add all changes to the staging area
git add .

# Commit the changes
git commit -m "The model of the reqrep and dealerrouter both works well."

# Push the new branch to the remote repository
git push origin feature/reqrep-dealerrouter

# Verify the branch has been pushed
git branch -r