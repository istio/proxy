import {
  Flex,
  Image,
  Link,
  Table,
} from '@chakra-ui/react'
import React from "react"

import {
  IRepoInfo,
  IUserLogin} from "../@types/app"


export const RelatedUserTableHeaders = () => {
  return (
    <>
      <Table.ColumnHeader>User</Table.ColumnHeader>
      <Table.ColumnHeader>Full name</Table.ColumnHeader>
    </>)
}

export const RelatedUserTr: React.FC<{resource: IRepoInfo | IUserLogin}> = ({resource}) => {
  const user = resource as IUserLogin
  return (
    <>
      <Table.Cell>
        <Flex align="center">
          <Image
            boxSize='2rem'
            borderRadius='full'
            alt="Avatar"
            mr='12px'
            src={user.avatar_url} />
          <Link href={user.html_url}>
            {user.login}
          </Link>
        </Flex>
      </Table.Cell>
      <Table.Cell>
        {user.name}
      </Table.Cell>
    </>)
}
