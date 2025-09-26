import {
  Flex,
  Link,
  Table,
} from '@chakra-ui/react'
import React from "react"

import {
  IRepoInfo,
  IUserLogin} from "../@types/app"


export const RepoTableHeaders = () => {
  return (
    <>
      <Table.ColumnHeader>Repo</Table.ColumnHeader>
      <Table.ColumnHeader>Updated</Table.ColumnHeader>
    </>)
}

export const RepoTr: React.FC<{resource: IRepoInfo | IUserLogin}> = ({resource}) => {
  const repo = resource as IRepoInfo
  return (
    <>
      <Table.Cell>
        <Flex align="center">
          <Link href={repo.html_url}>
            {repo.full_name}
          </Link>
        </Flex>
      </Table.Cell>
      <Table.Cell>{repo.updated_at}</Table.Cell>
    </>)
}
