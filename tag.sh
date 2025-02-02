#!/bin/bash

# Check if a tag name is provided
if [ -z "$1" ]; then
  echo "Usage: $0 <tag>"
  exit 1
fi

TAG=$1

# Delete the tag locally
git tag -d $TAG

# Delete the tag remotely
git push origin :refs/tags/$TAG

# Create the tag for the current commit
git tag $TAG

# Push the tag to the remote repository
git push origin $TAG

echo "Tag '$TAG' has been reset to the current commit and pushed to the remote repository."